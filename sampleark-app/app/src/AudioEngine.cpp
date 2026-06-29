#include "AudioEngine.h"

namespace
{
// Plays a one-shot from an in-memory AudioBuffer. Positionable so the transport can
// seek/start it instantly. Mono buffers play on all output channels.
class BufferSource : public juce::PositionableAudioSource
{
public:
    BufferSource (const juce::AudioBuffer<float>& b, juce::SpinLock& lk,
                  const std::atomic<int>* region, const std::atomic<bool>* loopFlag)
        : buffer (b), lock (lk), regionLen (region), looping (loopFlag) {}

    void setNextReadPosition (juce::int64 p) override { pos = p; }
    juce::int64 getNextReadPosition() const override  { return pos; }
    juce::int64 getTotalLength() const override        { return buffer.getNumSamples(); }
    bool isLooping() const override                    { return looping != nullptr && looping->load(); }

    void prepareToPlay (int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        // Try-lock: if the message thread is mid re-render, emit a block of silence rather
        // than read a half-written buffer (no glitch, just one quiet block).
        const juce::SpinLock::ScopedTryLockType sl (lock);
        const int total = buffer.getNumSamples();
        const int rlen = (regionLen != nullptr) ? regionLen->load() : total;
        const bool loop = isLooping() && rlen > 0;

        if (! sl.isLocked() || total == 0)
        {
            info.clearActiveBufferRegion();
            pos += info.numSamples;
            if (loop && rlen > 0) pos %= rlen;
            return;
        }

        const int srcChans = buffer.getNumChannels();
        for (int ch = 0; ch < info.buffer->getNumChannels(); ++ch)
        {
            const int srcCh = juce::jmin (ch, srcChans - 1);
            auto* dst = info.buffer->getWritePointer (ch, info.startSample);
            for (int i = 0; i < info.numSamples; ++i)
            {
                auto sp = pos + i;
                if (loop) sp %= rlen;
                dst[i] = (srcCh >= 0 && sp >= 0 && sp < total) ? buffer.getSample (srcCh, (int) sp) : 0.0f;
            }
        }
        pos += info.numSamples;
        if (loop) pos %= rlen;
    }

private:
    const juce::AudioBuffer<float>& buffer;
    juce::SpinLock& lock;
    const std::atomic<int>* regionLen = nullptr;
    const std::atomic<bool>* looping = nullptr;
    juce::int64 pos = 0;
};
}

namespace sa
{
AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    deviceManager.initialiseWithDefaultDevices (0, 2);
    sourcePlayer.setSource (&transport);
    deviceManager.addAudioCallback (&sourcePlayer);
    transport.addChangeListener (this);

    renderThread.startThread();
}

AudioEngine::~AudioEngine()
{
    renderThread.signalThreadShouldExit();
    renderSignal.signal();
    renderThread.stopThread (2000);

    transport.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    sourcePlayer.setSource (nullptr);
    transport.removeChangeListener (this);
}

bool AudioEngine::loadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    transport.stop();
    transport.setSource (nullptr);

    // Ensure the render worker isn't touching the buffers we're about to replace.
    renderDirty = false;
    while (renderBusy.load()) juce::Thread::sleep (1);

    const int numCh = (int) juce::jmax ((unsigned) 1, reader->numChannels);
    const int numSamps = (int) reader->lengthInSamples;
    sampleBuffer.setSize (numCh, numSamps);
    reader->read (&sampleBuffer, 0, numSamps, 0, true, true);

    fileSampleRate = reader->sampleRate;
    fileBits       = (int) reader->bitsPerSample;
    fileFrames     = reader->lengthInSamples;
    loadedName     = file.getFileName();

    prepParams = PrepParams{};                              // reset prep on new file

    // playBuffer is allocated once with headroom for effect tails (delay/reverb ring out
    // past the trimmed region, up to the 15 s cap). Prep writes region+tail into
    // [0, regionLen); the source stays attached so turning knobs never re-attaches it.
    const int capSamps = numSamps + (int) (15.0 * fileSampleRate);
    playBuffer.setSize (2, capSamps);       // render in stereo (mono sources play dual-mono;
    renderTemp.setSize (2, capSamps);       // stereo effects like ping-pong can spread it)
    playSource = std::make_unique<BufferSource> (playBuffer, playLock, &regionLen, &loopOn);
    transport.setSource (playSource.get(), 0, nullptr, fileSampleRate);
    requestRender();

    thumb.setSource (new juce::FileInputSource (file));

    if (onChange) onChange();
    return true;
}

void AudioEngine::requestRender()
{
    renderDirty = true;
    renderBusy = true;
    renderSignal.signal();
}

void AudioEngine::renderLoop()
{
    while (! renderThread.threadShouldExit())
    {
        renderSignal.wait (-1);
        while (renderDirty.exchange (false) && ! renderThread.threadShouldExit())
        {
            PrepParams p; FxRack r; double t;
            {
                const juce::ScopedLock sl (stateLock);   // quick snapshot, then render unlocked
                p = prepParams; r = fxRack; t = tempoBpm;
            }
            doRender (p, r, t);
            renderVer.fetch_add (1);
        }
        renderBusy = false;
    }
}

void AudioEngine::doRender (const PrepParams& prep, const FxRack& rack, double tempo)
{
    if (sampleBuffer.getNumSamples() == 0)
        return;

    const int total = sampleBuffer.getNumSamples();
    const int s = juce::jlimit (0, total - 1, (int) std::round (prep.startFrac * total));
    const int e = juce::jlimit (s + 1, total, (int) std::round (prep.endFrac * total));
    const int len = e - s;
    const int outCh = 2;                                  // always render stereo
    const int srcChs = sampleBuffer.getNumChannels();

    float g = (float) juce::Decibels::decibelsToGain (prep.gainDb);
    if (prep.normalize)
    {
        const float peak = sampleBuffer.getMagnitude (s, len);
        if (peak > 1.0e-9f)
            g *= (float) juce::Decibels::decibelsToGain (-1.0) / peak;
    }
    const int fi = juce::jlimit (0, len, (int) (prep.fadeInMs  * 0.001 * fileSampleRate));
    const int fo = juce::jlimit (0, len, (int) (prep.fadeOutMs * 0.001 * fileSampleRate));

    // Render region + tail headroom into the preallocated scratch (no per-render alloc).
    const int cap  = playBuffer.getNumSamples();
    const int rlen = juce::jmin (len + computeTailSamples (rack, tempo), cap);

    juce::AudioBuffer<float> temp (renderTemp.getArrayOfWritePointers(), outCh, 0, rlen);
    if (rlen > len) temp.clear (len, rlen - len);     // tail starts silent (region overwritten below)
    for (int c = 0; c < outCh; ++c)
        temp.copyFrom (c, 0, sampleBuffer, juce::jmin (c, srcChs - 1), s, len);  // dual-mono if source mono
    temp.applyGain (0, len, g);
    if (fi > 0) temp.applyGainRamp (0, fi, 0.0f, 1.0f);
    if (fo > 0) temp.applyGainRamp (len - fo, fo, 1.0f, 0.0f);
    rack.process (temp, fileSampleRate, tempo);        // effects ring the tail into [len, rlen)

    // Trim trailing silence (never below the region) + a short safety fade -> always ends at zero.
    int last = rlen - 1;
    while (last > len && temp.getMagnitude (last, 1) < 1.0e-4f)
        --last;
    const int finalLen = juce::jmax (len, last + 1);

    // Output fades (post-effect) over the whole result. Fade-out anchors to the dynamic
    // output end; a 3 ms minimum always guarantees a click-free ending.
    const int ofi = juce::jlimit (0, finalLen, (int) (prep.outFadeInMs * 0.001 * fileSampleRate));
    if (ofi > 0) temp.applyGainRamp (0, ofi, 0.0f, 1.0f);
    const int safety = juce::jmin (finalLen, (int) (0.003 * fileSampleRate));
    const int ofo = juce::jmax (safety, juce::jlimit (0, finalLen, (int) (prep.outFadeOutMs * 0.001 * fileSampleRate)));
    if (ofo > 0) temp.applyGainRamp (finalLen - ofo, ofo, 1.0f, 0.0f);

    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        for (int c = 0; c < outCh; ++c)
            playBuffer.copyFrom (c, 0, temp, c, 0, finalLen);   // [finalLen..) is stale but never read
        regionLen = finalLen;
    }
}

int AudioEngine::computeTailSamples (const FxRack& rack, double tempo) const
{
    constexpr double capSeconds = 15.0;
    double tailSec = 0.0;
    for (const auto& slot : rack.slots())
    {
        if (! slot.on) continue;
        if (slot.type == FxType::Delay && slot.params.size() > 4 && slot.params[4] > 0.001f) // mix > 0
        {
            const double dt = delayTimeSeconds (slot.params, tempo);
            const double fb = juce::jlimit (0.05, 0.95, (double) slot.params[1] * 0.95);
            const double reps = std::log (0.001) / std::log (fb);   // repeats to ~-60 dB
            tailSec = juce::jmax (tailSec, dt * reps);
        }
        // Reverb tail handled here when its DSP lands (M5).
    }
    return (int) (juce::jlimit (0.0, capSeconds, tailSec) * fileSampleRate);
}

void AudioEngine::selectSlot (int s)
{
    selSlot = juce::jlimit (0, kNumSlots - 1, s);
    if (onChange) onChange();
}

void AudioEngine::rackToggleBypass (int slot)
{
    { const juce::ScopedLock sl (stateLock); fxRack.toggleBypass (slot); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackMove (int from, int to)
{
    { const juce::ScopedLock sl (stateLock); fxRack.move (from, to); }
    if (selSlot == from) selSlot = to;
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackSetParam (int slot, int paramIndex, float value)
{
    { const juce::ScopedLock sl (stateLock); fxRack.setParam (slot, paramIndex, value); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::setTrim (double startFrac, double endFrac)
{
    {
        const juce::ScopedLock sl (stateLock);
        prepParams.startFrac = juce::jlimit (0.0, 0.98, startFrac);
        prepParams.endFrac   = juce::jlimit (prepParams.startFrac + 0.01, 1.0, endFrac);
    }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::setFadeInMs (double ms)    { { const juce::ScopedLock sl (stateLock); prepParams.fadeInMs    = juce::jmax (0.0, ms); } requestRender(); if (onChange) onChange(); }
void AudioEngine::setFadeOutMs (double ms)   { { const juce::ScopedLock sl (stateLock); prepParams.fadeOutMs   = juce::jmax (0.0, ms); } requestRender(); if (onChange) onChange(); }
void AudioEngine::setOutFadeInMs (double ms) { { const juce::ScopedLock sl (stateLock); prepParams.outFadeInMs  = juce::jmax (0.0, ms); } requestRender(); if (onChange) onChange(); }
void AudioEngine::setOutFadeOutMs (double ms){ { const juce::ScopedLock sl (stateLock); prepParams.outFadeOutMs = juce::jmax (0.0, ms); } requestRender(); if (onChange) onChange(); }
void AudioEngine::setGainDb (double db)    { { const juce::ScopedLock sl (stateLock); prepParams.gainDb = db; } requestRender(); if (onChange) onChange(); }
void AudioEngine::setNormalize (bool on)   { { const juce::ScopedLock sl (stateLock); prepParams.normalize = on; } requestRender(); if (onChange) onChange(); }

bool AudioEngine::exportPreppedTo (const juce::File& file, int bitDepth)
{
    while (renderBusy.load()) juce::Thread::sleep (1);   // export the latest render
    const int len = regionLen.load();
    if (len <= 0)
        return false;

    // Snapshot the prepped region under the lock, then write without holding it.
    juce::AudioBuffer<float> out (playBuffer.getNumChannels(), len);
    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        for (int c = 0; c < out.getNumChannels(); ++c)
            out.copyFrom (c, 0, playBuffer, c, 0, len);
    }

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (os.get(), fileSampleRate,
                             (unsigned int) out.getNumChannels(), bitDepth, {}, 0));
    if (writer == nullptr)
        return false;

    os.release();   // writer owns the stream now
    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    return true;
}

std::vector<float> AudioEngine::outputPeaks (int numColumns) const
{
    std::vector<float> peaks ((size_t) juce::jmax (1, numColumns), 0.0f);
    const int n = regionLen.load();
    if (n <= 0 || numColumns <= 0)
        return peaks;

    const int ch = playBuffer.getNumChannels();
    for (int col = 0; col < numColumns; ++col)
    {
        const long long a = (long long) col * n / numColumns;
        const long long b = (long long) (col + 1) * n / numColumns;
        float peak = 0.0f;
        for (long long i = a; i < b && i < n; ++i)
            for (int c = 0; c < ch; ++c)
                peak = juce::jmax (peak, std::abs (playBuffer.getSample (c, (int) i)));
        peaks[(size_t) col] = juce::jmin (1.0f, peak);
    }
    return peaks;
}

void AudioEngine::play()
{
    if (! hasFile()) return;
    transport.setPosition (0.0);
    transport.start();
}

void AudioEngine::stop()
{
    transport.stop();
    transport.setPosition (0.0);
}

void AudioEngine::togglePlay()
{
    if (transport.isPlaying()) stop();
    else                       play();
}

void AudioEngine::setLoop (bool shouldLoop)
{
    loopOn = shouldLoop;
    if (shouldLoop && ! transport.isPlaying())
        play();
    if (onChange) onChange();
}

bool AudioEngine::isPlaying() const
{
    return transport.isPlaying();
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (onChange) onChange();
}
}
