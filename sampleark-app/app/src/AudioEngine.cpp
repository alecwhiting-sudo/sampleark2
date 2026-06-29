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
            PrepParams p; FxRack r; double t; std::array<Transformer, kNumTransformers> tr;
            {
                const juce::ScopedLock sl (stateLock);   // quick snapshot, then render unlocked
                p = prepParams; r = fxRack; t = tempoBpm; tr = transformerArray;
            }
            doRender (p, r, t, tr);
            renderVer.fetch_add (1);
        }
        renderBusy = false;
    }
}

static float transPhase (const sa::Transformer& t, int pos, double outLen, double sr, double tempo)
{
    if (t.basis == 0)
        return outLen > 0.0 ? (float) (pos / outLen) : 0.0f;          // one-shot over the output
    const double cycSec = (t.rateDiv <= 0) ? 1.0 / juce::jmax (0.01f, t.freqHz)   // free Hz
                                           : sa::transRateSeconds (t.rateDiv, tempo);
    const double cyc = cycSec * sr;
    if (cyc <= 0.0) return 0.0f;
    const double ph = pos / cyc + (double) t.phase;                   // phase offset
    return (float) (ph - std::floor (ph));
}

void AudioEngine::doRender (const PrepParams& prep, const FxRack& rack, double tempo,
                            const std::array<Transformer, kNumTransformers>& trans)
{
    const int finalLen = renderInto (prep, rack, trans, tempo, renderTemp);
    if (finalLen <= 0)
        return;
    const juce::SpinLock::ScopedLockType sl (playLock);
    for (int c = 0; c < playBuffer.getNumChannels(); ++c)
        playBuffer.copyFrom (c, 0, renderTemp, c, 0, finalLen);   // [finalLen..) stale but never read
    regionLen = finalLen;
}

// Raised-cosine (equal-power-ish) fade-out over the last `n` samples ending at sample `endExclusive`.
// Unlike a linear ramp it has zero slope at both ends, so it never leaves a derivative kink or a
// non-zero final sample -> no click when the one-shot is imported into another app.
static void cosineFadeOut (juce::AudioBuffer<float>& buf, int endExclusive, int n)
{
    n = juce::jmin (n, endExclusive);
    if (n <= 0) return;
    const int start = endExclusive - n;
    for (int k = 0; k < n; ++k)
    {
        // w: 1 -> 0 across the window, hitting exactly 0 on the final sample.
        const float w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (k + 1) / (float) n);
        for (int c = 0; c < buf.getNumChannels(); ++c)
            buf.getWritePointer (c)[start + k] *= w;
    }
}

// Shared render core: writes the prepped region + effect tail into `work` (>= region+tail, 2ch)
// and returns the trimmed length. Used by the live render (doRender) and the M4 variations.
int AudioEngine::renderInto (const PrepParams& prep, const FxRack& rack,
                             const std::array<Transformer, kNumTransformers>& trans, double tempo,
                             juce::AudioBuffer<float>& work) const
{
    if (sampleBuffer.getNumSamples() == 0 || work.getNumChannels() < 2)
        return 0;

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

    // Render region + tail headroom into the provided work buffer (bounded by its capacity).
    const int cap  = work.getNumSamples();
    const int rlen = juce::jmin (len + computeTailSamples (rack, tempo), cap);

    juce::AudioBuffer<float> temp (work.getArrayOfWritePointers(), outCh, 0, rlen);
    if (rlen > len) temp.clear (len, rlen - len);     // tail starts silent (region overwritten below)
    for (int c = 0; c < outCh; ++c)
        temp.copyFrom (c, 0, sampleBuffer, juce::jmin (c, srcChs - 1), s, len);  // dual-mono if source mono
    temp.applyGain (0, len, g);
    if (fi > 0) temp.applyGainRamp (0, fi, 0.0f, 1.0f);
    if (fo > 0) temp.applyGainRamp (len - fo, fo, 1.0f, 0.0f);

    // Build time-varying modulation from the transformers (evaluated per block in the rack).
    // One-shot transformers span the WHOLE rendered output (region + effect tail), matching the
    // transformer graph's full-width alignment with the OUTPUT waveform — so a drawn curve plays
    // out across the entire sound, delay/reverb tail included. (Cyclic transformers ignore outLen.)
    auto trs = std::make_shared<std::array<Transformer, kNumTransformers>> (trans);
    const double outLen = (double) rlen, sr = fileSampleRate, tp = tempo;
    FxModulation mod;
    mod.paramAdd = [trs, outLen, sr, tp] (int slot, int param, int pos) -> float
    {
        float add = 0.0f;
        for (const auto& t : *trs)
            if (t.on && t.kind == TransTarget::EffectParam && t.slot == slot && t.param == param)
                add += t.depth * (shapeEval (t, transPhase (t, pos, outLen, sr, tp)) * 2.0f - 1.0f);
        return add;
    };
    mod.preGain = [trs, outLen, sr, tp] (int pos) -> float
    {
        float gn = 1.0f;
        for (const auto& t : *trs)
            if (t.on && t.kind == TransTarget::PreAmp)
                gn *= 1.0f - t.depth * (1.0f - shapeEval (t, transPhase (t, pos, outLen, sr, tp)));
        return gn;
    };
    mod.postGain = [trs, outLen, sr, tp] (int pos) -> float
    {
        float gn = 1.0f;
        for (const auto& t : *trs)
            if (t.on && t.kind == TransTarget::PostAmp)
                gn *= 1.0f - t.depth * (1.0f - shapeEval (t, transPhase (t, pos, outLen, sr, tp)));
        return gn;
    };

    rack.process (temp, fileSampleRate, tempo, mod);   // effects ring the tail into [len, rlen)

    // Trim trailing silence (never below the region) + a short safety fade -> always ends at zero.
    int last = rlen - 1;
    while (last > len && temp.getMagnitude (last, 1) < 1.0e-4f)
        --last;
    const int finalLen = juce::jmax (len, last + 1);

    // Output fades (post-effect) over the whole result. Fade-out anchors to the dynamic
    // output end; a raised-cosine fade with a 5 ms minimum guarantees a click-free ending even
    // for bass-heavy content (a 3 ms linear ramp was too short / left a slope kink).
    const int ofi = juce::jlimit (0, finalLen, (int) (prep.outFadeInMs * 0.001 * fileSampleRate));
    if (ofi > 0) temp.applyGainRamp (0, ofi, 0.0f, 1.0f);
    const int safety = juce::jmin (finalLen, (int) (0.005 * fileSampleRate));
    const int ofo = juce::jmax (safety, juce::jlimit (0, finalLen, (int) (prep.outFadeOutMs * 0.001 * fileSampleRate)));
    cosineFadeOut (temp, finalLen, ofo);

    return finalLen;
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
        if (slot.type == FxType::Reverb && slot.params.size() > 2 && slot.params[2] > 0.001f) // mix > 0
        {
            const double size  = juce::jlimit (0.0f, 1.0f, slot.params[0]);
            const double decay = slot.params.size() > 1 ? juce::jlimit (0.0f, 1.0f, slot.params[1]) : 0.5;
            tailSec = juce::jmax (tailSec, 0.6 + size * 1.4 + decay * 2.0);   // ~0.6 .. 4 s
        }
    }
    return (int) (juce::jlimit (0.0, capSeconds, tailSec) * fileSampleRate);
}

int AudioEngine::renderState (const PrepParams& prep, const FxRack& rack,
                              const std::array<Transformer, kNumTransformers>& trans, double tempo,
                              juce::AudioBuffer<float>& out) const
{
    if (sampleBuffer.getNumSamples() == 0)
        return 0;
    const int total = sampleBuffer.getNumSamples();
    const int s = juce::jlimit (0, total - 1, (int) std::round (prep.startFrac * total));
    const int e = juce::jlimit (s + 1, total, (int) std::round (prep.endFrac * total));
    const int need = (e - s) + computeTailSamples (rack, tempo);
    out.setSize (2, juce::jmax (1, need), false, false, true);
    return renderInto (prep, rack, trans, tempo, out);
}

bool AudioEngine::isSlotModulated (int slot) const
{
    for (const auto& t : transformerArray)
        if (t.on && t.kind == TransTarget::EffectParam && t.slot == slot)
            return true;
    return false;
}

std::vector<float> AudioEngine::liveParams (int slot) const
{
    slot = juce::jlimit (0, kNumSlots - 1, slot);
    const auto& base = fxRack.slots()[(size_t) slot].params;
    std::vector<float> out (base.begin(), base.end());
    if (! isPlaying() || fileSampleRate <= 0.0) return out;

    // Mirror the renderer's additive modulation at the live playhead. Use the SAME span as the
    // render (region + effect tail = rlen) so a one-shot's on-screen graph matches what was baked
    // into the audio across the whole output.
    const int    total  = sampleBuffer.getNumSamples();
    const int    sSamp  = juce::jlimit (0, juce::jmax (0, total - 1), (int) std::round (prepParams.startFrac * total));
    const int    eSamp  = juce::jlimit (sSamp + 1, total, (int) std::round (prepParams.endFrac * total));
    const int    cap    = juce::jmax (1, playBuffer.getNumSamples());
    const double outLen = (double) juce::jmin ((eSamp - sSamp) + computeTailSamples (fxRack, tempoBpm), cap);
    const int    pos    = (int) (transport.getCurrentPosition() * fileSampleRate);
    for (int pi = 0; pi < (int) out.size(); ++pi)
    {
        float add = 0.0f;
        for (const auto& t : transformerArray)
            if (t.on && t.kind == TransTarget::EffectParam && t.slot == slot && t.param == pi)
                add += t.depth * (shapeEval (t, transPhase (t, pos, outLen, fileSampleRate, tempoBpm)) * 2.0f - 1.0f);
        out[(size_t) pi] = juce::jlimit (0.0f, 1.0f, out[(size_t) pi] + add);
    }
    return out;
}

void AudioEngine::auditionBuffer (const juce::AudioBuffer<float>& buf, int len)
{
    const int n = juce::jmin (len, buf.getNumSamples(), playBuffer.getNumSamples());
    if (n <= 0) return;
    {
        const juce::SpinLock::ScopedLockType sl (playLock);
        for (int c = 0; c < playBuffer.getNumChannels(); ++c)
            playBuffer.copyFrom (c, 0, buf, juce::jmin (c, buf.getNumChannels() - 1), 0, n);
        regionLen = n;
    }
    play();
}

bool AudioEngine::writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf, int len, int bitDepth) const
{
    const int n = juce::jmin (len, buf.getNumSamples());
    if (n <= 0) return false;
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (os.get(), fileSampleRate, (unsigned int) buf.getNumChannels(), bitDepth, {}, 0));
    if (writer == nullptr) return false;
    os.release();
    writer->writeFromAudioSampleBuffer (buf, 0, n);
    return true;
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
    from = juce::jlimit (0, kNumSlots - 1, from);
    to   = juce::jlimit (0, kNumSlots - 1, to);

    // Reordering rotates the slot array, so every slot index in (from, to] (or [to, from))
    // shifts by one and `from` lands on `to`. Anything that references an effect by slot index
    // — the transformers' targets and the current selection — must follow the same permutation,
    // otherwise a moved effect's transformer "loses its grip" and modulates the wrong slot.
    auto remap = [from, to] (int s) -> int
    {
        if (s == from) return to;
        if (from < to) return (s > from && s <= to) ? s - 1 : s;
        return                (s >= to && s < from) ? s + 1 : s;
    };

    {
        const juce::ScopedLock sl (stateLock);
        fxRack.move (from, to);
        for (auto& t : transformerArray)
            if (t.kind == TransTarget::EffectParam)
                t.slot = remap (t.slot);
    }
    selSlot = remap (selSlot);
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackSetParam (int slot, int paramIndex, float value)
{
    { const juce::ScopedLock sl (stateLock); fxRack.setParam (slot, paramIndex, value); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::setTransformer (int index, const Transformer& t)
{
    if (index < 0 || index >= kNumTransformers) return;
    { const juce::ScopedLock sl (stateLock); transformerArray[(size_t) index] = t; }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::applyRecipe (const PrepParams& p, const FxRack& r,
                               const std::array<Transformer, kNumTransformers>& t)
{
    {
        const juce::ScopedLock sl (stateLock);
        prepParams      = p;
        fxRack          = r;
        transformerArray = t;
    }
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

std::vector<float> AudioEngine::outputPeaks (int numColumns) const { return outputPeaks (numColumns, -1); }

std::vector<float> AudioEngine::outputPeaks (int numColumns, int channel) const
{
    std::vector<float> peaks ((size_t) juce::jmax (1, numColumns), 0.0f);
    const int n  = regionLen.load();
    const int ch = playBuffer.getNumChannels();
    if (n <= 0 || numColumns <= 0 || ch <= 0)
        return peaks;

    for (int col = 0; col < numColumns; ++col)
    {
        const long long a = (long long) col * n / numColumns;
        const long long b = (long long) (col + 1) * n / numColumns;
        float peak = 0.0f;
        for (long long i = a; i < b && i < n; ++i)
        {
            if (channel >= 0 && channel < ch)
                peak = juce::jmax (peak, std::abs (playBuffer.getSample (channel, (int) i)));
            else
                for (int c = 0; c < ch; ++c)
                    peak = juce::jmax (peak, std::abs (playBuffer.getSample (c, (int) i)));
        }
        peaks[(size_t) col] = juce::jmin (1.0f, peak);
    }
    return peaks;
}

std::vector<float> AudioEngine::sourcePeaks (int numColumns, int channel) const
{
    std::vector<float> peaks ((size_t) juce::jmax (1, numColumns), 0.0f);
    const int n  = sampleBuffer.getNumSamples();
    const int ch = sampleBuffer.getNumChannels();
    if (n <= 0 || ch <= 0 || numColumns <= 0)
        return peaks;

    for (int col = 0; col < numColumns; ++col)
    {
        const long long a = (long long) col * n / numColumns;
        const long long b = (long long) (col + 1) * n / numColumns;
        float peak = 0.0f;
        for (long long i = a; i < b && i < n; ++i)
        {
            if (channel >= 0 && channel < ch)
                peak = juce::jmax (peak, std::abs (sampleBuffer.getSample (channel, (int) i)));
            else
                for (int c = 0; c < ch; ++c)
                    peak = juce::jmax (peak, std::abs (sampleBuffer.getSample (c, (int) i)));
        }
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
