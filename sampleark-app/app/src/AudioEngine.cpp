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
}

AudioEngine::~AudioEngine()
{
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

    const int numCh = (int) juce::jmax ((unsigned) 1, reader->numChannels);
    const int numSamps = (int) reader->lengthInSamples;
    sampleBuffer.setSize (numCh, numSamps);
    reader->read (&sampleBuffer, 0, numSamps, 0, true, true);

    fileSampleRate = reader->sampleRate;
    fileBits       = (int) reader->bitsPerSample;
    fileFrames     = reader->lengthInSamples;
    loadedName     = file.getFileName();

    prepParams = PrepParams{};                              // reset prep on new file

    // playBuffer is allocated once at full length; prep writes the region into [0, regionLen)
    // and zeroes the rest. The source stays attached — only the contents change — so turning
    // knobs never re-attaches/re-prepares the transport (that was the glitch source).
    playBuffer.setSize (numCh, numSamps);
    playSource = std::make_unique<BufferSource> (playBuffer, playLock, &regionLen, &loopOn);
    transport.setSource (playSource.get(), 0, nullptr, fileSampleRate);
    renderPrep();

    thumb.setSource (new juce::FileInputSource (file));

    if (onChange) onChange();
    return true;
}

void AudioEngine::renderPrep()
{
    if (sampleBuffer.getNumSamples() == 0)
        return;

    const int total = sampleBuffer.getNumSamples();
    const int s = juce::jlimit (0, total - 1, (int) std::round (prepParams.startFrac * total));
    const int e = juce::jlimit (s + 1, total, (int) std::round (prepParams.endFrac * total));
    const int len = e - s;
    const int ch = sampleBuffer.getNumChannels();

    // Compute gain (incl. normalize from the source region) before taking the lock.
    float g = (float) juce::Decibels::decibelsToGain (prepParams.gainDb);
    if (prepParams.normalize)
    {
        const float peak = sampleBuffer.getMagnitude (s, len);
        if (peak > 1.0e-9f)
            g *= (float) juce::Decibels::decibelsToGain (-1.0) / peak;
    }
    const int fi = juce::jlimit (0, len, (int) (prepParams.fadeInMs  * 0.001 * fileSampleRate));
    const int fo = juce::jlimit (0, len, (int) (prepParams.fadeOutMs * 0.001 * fileSampleRate));

    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        playBuffer.clear();
        for (int c = 0; c < ch; ++c)
            playBuffer.copyFrom (c, 0, sampleBuffer, c, s, len);
        playBuffer.applyGain (0, len, g);
        if (fi > 0) playBuffer.applyGainRamp (0, fi, 0.0f, 1.0f);
        if (fo > 0) playBuffer.applyGainRamp (len - fo, fo, 1.0f, 0.0f);
        regionLen = len;
    }
}

void AudioEngine::setTrim (double startFrac, double endFrac)
{
    prepParams.startFrac = juce::jlimit (0.0, 0.98, startFrac);
    prepParams.endFrac   = juce::jlimit (prepParams.startFrac + 0.01, 1.0, endFrac);
    renderPrep();
    if (onChange) onChange();
}

void AudioEngine::setFadeInMs (double ms)  { prepParams.fadeInMs  = juce::jmax (0.0, ms); renderPrep(); if (onChange) onChange(); }
void AudioEngine::setFadeOutMs (double ms) { prepParams.fadeOutMs = juce::jmax (0.0, ms); renderPrep(); if (onChange) onChange(); }
void AudioEngine::setGainDb (double db)    { prepParams.gainDb = db; renderPrep(); if (onChange) onChange(); }
void AudioEngine::setNormalize (bool on)   { prepParams.normalize = on; renderPrep(); if (onChange) onChange(); }

bool AudioEngine::exportPreppedTo (const juce::File& file, int bitDepth)
{
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
