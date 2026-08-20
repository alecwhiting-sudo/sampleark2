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
    undoStack.clear();                                      // history belongs to the loaded sample
    redoStack.clear();

    // playBuffer is allocated once with headroom for effect tails (delay/reverb ring out
    // past the trimmed region, up to the 15 s cap). Prep writes region+tail into
    // [0, regionLen); the source stays attached so turning knobs never re-attaches it.
    const int capSamps = numSamps + (int) (15.0 * fileSampleRate);
    playBuffer.setSize (2, capSamps);       // render in stereo (mono sources play dual-mono;
    renderTemp.setSize (2, capSamps);       // stereo effects like ping-pong can spread it)
    // Retained tail: the cap plus the 50 ms pre-cut margin. Fixed size, so a long source doesn't
    // cost a third source-sized buffer.
    const int tailSamps = (int) ((kTailCapSeconds + 0.1) * fileSampleRate);
    tailBuffer.setSize (2, tailSamps);
    tailTemp.setSize (2, tailSamps);
    tailBuffer.clear(); tailTemp.clear();
    tailLen = 0; tailStart = 0;

    const int meterFrames = capSamps / kMeterHop + 1;   // preallocate metering envelopes (no worker alloc)
    for (auto* set : { &slotMeters, &renderMeters })
        for (auto& m : *set) { m.outDb.assign ((size_t) meterFrames, -120.0f); m.grDb.assign ((size_t) meterFrames, 0.0f); m.frames = 0; }
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
    // Capture dynamics metering into the worker's scratch envelopes (one frame per kMeterHop
    // samples, max-combined within a frame). Swapped into slotMeters under playLock below.
    for (auto& m : renderMeters) m.frames = 0;
    FxMeterSink sink;
    sink.capture = [this] (int slot, int startSample, float outPeak, float grDb)
    {
        if (slot < 0 || slot >= kNumSlots) return;
        auto& m = renderMeters[(size_t) slot];
        const int f = startSample / kMeterHop;
        if (f < 0 || f >= (int) m.outDb.size()) return;
        const float outDbV = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, outPeak));
        if (f >= m.frames) { m.outDb[(size_t) f] = outDbV; m.grDb[(size_t) f] = grDb; m.frames = f + 1; }
        else { m.outDb[(size_t) f] = juce::jmax (m.outDb[(size_t) f], outDbV); m.grDb[(size_t) f] = juce::jmax (m.grDb[(size_t) f], grDb); }
    };

    TailCapture cap { &tailTemp, 0, 0 };
    const int finalLen = renderInto (prep, rack, trans, tempo, renderTemp, &sink, &cap);
    if (finalLen <= 0)
        return;
    const juce::SpinLock::ScopedLockType sl (playLock);
    for (int c = 0; c < playBuffer.getNumChannels(); ++c)
        playBuffer.copyFrom (c, 0, renderTemp, c, 0, finalLen);   // [finalLen..) stale but never read
    for (int i = 0; i < kNumSlots; ++i)
        std::swap (slotMeters[(size_t) i], renderMeters[(size_t) i]);   // publish (vectors keep their alloc)
    std::swap (tailBuffer, tailTemp);                             // publish the retained tail (no copy)
    tailStart = cap.startInOutput;
    tailLen   = cap.len;
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
                             juce::AudioBuffer<float>& work, const FxMeterSink* meter,
                             TailCapture* tail) const
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

    // Two lengths, deliberately separate (see FxRack.h TailMode):
    //   wlen     - what we COMPUTE: the full natural ring-out, always, whatever the tail modes say,
    //              plus any manually requested length that reaches beyond it.
    //   outLimit - what we PUBLISH: region + the longest tail grant. Everything downstream
    //              (playback, loop, OUTPUT waveform, export, variations, WRITE) keys off this.
    const int cap = work.getNumSamples();
    const double regionSecs = fileSampleRate > 0.0 ? len / fileSampleRate : 0.0;
    const TailGrant grant = rack.outputGrant (tempo, regionSecs);
    const int natEnd   = juce::jmin (len + renderTailSamples (rack, tempo), cap);
    const int outLimit = juce::jmin (len + grantSamples (grant), cap);
    const int wlen     = juce::jmax (natEnd, outLimit);

    juce::AudioBuffer<float> temp (work.getArrayOfWritePointers(), outCh, 0, wlen);
    if (wlen > len) temp.clear (len, wlen - len);     // tail starts silent (region overwritten below)
    for (int c = 0; c < outCh; ++c)
        temp.copyFrom (c, 0, sampleBuffer, juce::jmin (c, srcChs - 1), s, len);  // dual-mono if source mono
    temp.applyGain (0, len, g);
    if (fi > 0) temp.applyGainRamp (0, fi, 0.0f, 1.0f);
    if (fo > 0) temp.applyGainRamp (len - fo, fo, 1.0f, 0.0f);

    // Build time-varying modulation from the transformers (evaluated per block in the rack).
    // One-shot transformers span the WHOLE rendered output (region + effect tail), matching the
    // transformer graph's full-width alignment with the OUTPUT waveform — so a drawn curve plays
    // out across the entire sound, delay/reverb tail included. (Cyclic transformers ignore outLen.)
    // The span is the OUTPUT length (not the render window): a one-shot curve has to play out
    // across exactly the sound the user ends up with, or the drawn graph and the audio disagree.
    // liveParams() mirrors this same value — the two must always move together.
    auto trs = std::make_shared<std::array<Transformer, kNumTransformers>> (trans);
    const double outLen = (double) outLimit, sr = fileSampleRate, tp = tempo;
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

    rack.process (temp, fileSampleRate, tempo, mod, meter);   // effects ring the tail into [len, wlen)

    // Publish up to outLimit. An exact grant (TIME / xLOOP) is the length, full stop — no silence
    // trimming, or the padding that makes lengths uniform would be undone immediately. A natural
    // (FULL) tail still gets trimmed back to where it actually goes quiet.
    int finalLen = juce::jmax (len, outLimit);
    if (! grant.exact)
    {
        int last = juce::jmin (outLimit, wlen) - 1;
        while (last > len && temp.getMagnitude (last, 1) < 1.0e-4f)
            --last;
        finalLen = juce::jlimit (len, outLimit, last + 1);
    }

    // Retain the ring-out we are about to cut off, pristine (pre-fade), starting a little before
    // the cut so a future feature can splice or crossfade across it rather than butt-joining.
    if (tail != nullptr && tail->buffer != nullptr && tail->buffer->getNumSamples() > 0)
    {
        int tailEnd = wlen;                                  // ignore trailing silence, so a FULL
        while (tailEnd > finalLen && temp.getMagnitude (tailEnd - 1, 1) < 1.0e-4f)
            --tailEnd;                                       // tail doesn't ghost its own dead air
        const int margin = (int) (0.050 * fileSampleRate);
        const int start  = juce::jmax (0, finalLen - margin);
        const int n      = juce::jmin (tailEnd - start, tail->buffer->getNumSamples());
        for (int c = 0; c < tail->buffer->getNumChannels(); ++c)
            tail->buffer->copyFrom (c, 0, temp, juce::jmin (c, outCh - 1), start, juce::jmax (0, n));
        tail->startInOutput = start;
        tail->len = juce::jmax (0, n);
    }

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

// The full ring-out, ignoring tail modes entirely — this is always computed, so the material
// exists whatever the output length ends up being.
int AudioEngine::renderTailSamples (const FxRack& rack, double tempo) const
{
    return (int) (rack.naturalTail (tempo) * fileSampleRate);
}

int AudioEngine::grantSamples (const TailGrant& g) const
{
    return (int) (juce::jlimit (0.0, kTailCapSeconds, g.seconds) * fileSampleRate);
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
    const double regionSecs = fileSampleRate > 0.0 ? (e - s) / fileSampleRate : 0.0;
    const int need = (e - s) + juce::jmax (renderTailSamples (rack, tempo),
                                           grantSamples (rack.outputGrant (tempo, regionSecs)));
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
    const int    rlen   = eSamp - sSamp;
    const double regSec = rlen / fileSampleRate;
    const double outLen = (double) juce::jmin (rlen + grantSamples (fxRack.outputGrant (tempoBpm, regSec)), cap);
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

bool AudioEngine::hasMeter (int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return false;
    const juce::SpinLock::ScopedLockType sl (playLock);
    return slotMeters[(size_t) slot].frames > 0;
}

float AudioEngine::meterOutDbAt (int slot, int posSamples) const
{
    if (slot < 0 || slot >= kNumSlots) return -120.0f;
    const juce::SpinLock::ScopedLockType sl (playLock);
    const auto& m = slotMeters[(size_t) slot];
    if (m.frames <= 0) return -120.0f;
    const int f = juce::jlimit (0, m.frames - 1, posSamples / kMeterHop);
    return m.outDb[(size_t) f];
}

float AudioEngine::meterGrDbAt (int slot, int posSamples) const
{
    if (slot < 0 || slot >= kNumSlots) return 0.0f;
    const juce::SpinLock::ScopedLockType sl (playLock);
    const auto& m = slotMeters[(size_t) slot];
    if (m.frames <= 0) return 0.0f;
    const int f = juce::jlimit (0, m.frames - 1, posSamples / kMeterHop);
    return m.grDb[(size_t) f];
}

float AudioEngine::meterPeakOutDb (int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return -120.0f;
    const juce::SpinLock::ScopedLockType sl (playLock);
    const auto& m = slotMeters[(size_t) slot];
    float pk = -120.0f;
    for (int f = 0; f < m.frames; ++f) pk = juce::jmax (pk, m.outDb[(size_t) f]);
    return pk;
}

float AudioEngine::meterMaxGrDb (int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return 0.0f;
    const juce::SpinLock::ScopedLockType sl (playLock);
    const auto& m = slotMeters[(size_t) slot];
    float mx = 0.0f;
    for (int f = 0; f < m.frames; ++f) mx = juce::jmax (mx, m.grDb[(size_t) f]);
    return mx;
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

// ===================== edit history (undo/redo) =====================
// Gesture tags. A drag fires its setter dozens of times a second; changes carrying the SAME tag
// within kCoalesceMs collapse into the one step taken when the gesture began, so undo steps back
// a whole knob turn rather than one pixel of it. Discrete actions use tagNone: never coalesced.
namespace
{
enum : int { tagNone = 0, tagTrim, tagFadeIn, tagFadeOut, tagOutFadeIn, tagOutFadeOut, tagGain };
int tagRackParam  (int slot, int pi) { return 1000 + slot * 32 + pi; }
int tagTailAmount (int slot)         { return 2000 + slot; }
int tagTransformer (int index)       { return 3000 + index; }
}

juce::String AudioEngine::slotName (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return {};
    return fxInfo (fxRack.slots()[(size_t) slot].type).name;
}

juce::String AudioEngine::paramName (int slot, int paramIndex) const
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots)) return {};
    const auto& info = fxInfo (fxRack.slots()[(size_t) slot].type);
    if (! juce::isPositiveAndBelow (paramIndex, (int) info.params.size())) return slotName (slot);
    return slotName (slot) + " " + info.params[(size_t) paramIndex].label;
}

AudioEngine::EditState AudioEngine::captureState() const
{
    const juce::ScopedLock sl (stateLock);
    return { prepParams, fxRack, transformerArray };
}

void AudioEngine::applyState (const EditState& s)
{
    {
        const juce::ScopedLock sl (stateLock);
        prepParams       = s.prep;
        fxRack           = s.rack;
        transformerArray = s.trans;
    }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::recordEdit (int tag, const juce::String& desc)
{
    const juce::uint32 now = juce::Time::getMillisecondCounter();
    redoStack.clear();                       // a fresh edit abandons the redo chain

    if (tag != tagNone && ! undoStack.empty() && undoStack.back().tag == tag
        && now - undoStack.back().stamp < kCoalesceMs)
    {
        undoStack.back().stamp = now;        // same gesture still running: keep its opening snapshot
        return;
    }

    undoStack.push_back ({ captureState(), desc, tag, now });
    if ((int) undoStack.size() > kHistoryLimit)
        undoStack.pop_front();               // oldest step falls off the end
}

bool AudioEngine::undo()
{
    if (undoStack.empty()) return false;
    HistoryStep step = std::move (undoStack.back());
    undoStack.pop_back();
    redoStack.push_back ({ captureState(), step.desc, tagNone, 0 });   // where we were becomes the redo
    if ((int) redoStack.size() > kHistoryLimit) redoStack.pop_front();
    applyState (step.state);
    return true;
}

bool AudioEngine::redo()
{
    if (redoStack.empty()) return false;
    HistoryStep step = std::move (redoStack.back());
    redoStack.pop_back();
    undoStack.push_back ({ captureState(), step.desc, tagNone, 0 });
    if ((int) undoStack.size() > kHistoryLimit) undoStack.pop_front();
    applyState (step.state);
    return true;
}

void AudioEngine::selectSlot (int s)
{
    selSlot = juce::jlimit (0, kNumSlots - 1, s);
    if (onChange) onChange();
}

void AudioEngine::rackToggleBypass (int slot)
{
    recordEdit (tagNone, (fxRack.slots()[(size_t) juce::jlimit (0, kNumSlots - 1, slot)].on ? "Bypass " : "Enable ") + slotName (slot));
    { const juce::ScopedLock sl (stateLock); fxRack.toggleBypass (slot); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackMove (int from, int to)
{
    from = juce::jlimit (0, kNumSlots - 1, from);
    to   = juce::jlimit (0, kNumSlots - 1, to);
    if (from == to) return;                       // no-op drag: nothing to render or record
    recordEdit (tagNone, "Move " + slotName (from));

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
    // A drag that doesn't actually move the value must not render or land in the history, or undo
    // appears to do nothing.
    if (juce::isPositiveAndBelow (slot, kNumSlots)
        && juce::isPositiveAndBelow (paramIndex, (int) fxRack.slots()[(size_t) slot].params.size())
        && fxRack.slots()[(size_t) slot].params[(size_t) paramIndex] == value)
        return;

    recordEdit (tagRackParam (slot, paramIndex), paramName (slot, paramIndex));
    { const juce::ScopedLock sl (stateLock); fxRack.setParam (slot, paramIndex, value); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackSetTailMode (int slot, TailMode m)
{
    if (juce::isPositiveAndBelow (slot, kNumSlots) && fxRack.slots()[(size_t) slot].tailMode == m)
        return;
    recordEdit (tagNone, slotName (slot) + " Tail " + tailModeName (m));
    { const juce::ScopedLock sl (stateLock); fxRack.setTailMode (slot, m); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::rackSetTailAmount (int slot, float amount01)
{
    if (juce::isPositiveAndBelow (slot, kNumSlots) && fxRack.slots()[(size_t) slot].tailAmount == amount01)
        return;
    recordEdit (tagTailAmount (slot), slotName (slot) + " Tail length");
    { const juce::ScopedLock sl (stateLock); fxRack.setTailAmount (slot, amount01); }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::setTransformer (int index, const Transformer& t)
{
    if (index < 0 || index >= kNumTransformers) return;
    recordEdit (tagTransformer (index), "Transformer " + juce::String (index + 1));
    { const juce::ScopedLock sl (stateLock); transformerArray[(size_t) index] = t; }
    requestRender();
    if (onChange) onChange();
}

void AudioEngine::applyRecipe (const PrepParams& p, const FxRack& r,
                               const std::array<Transformer, kNumTransformers>& t)
{
    recordEdit (tagNone, "Recall");   // losing a hand-built rack to a click is exactly what undo is for
    applyState ({ p, r, t });
}

void AudioEngine::setTrim (double startFrac, double endFrac)
{
    const double s = juce::jlimit (0.0, 0.98, startFrac);
    const double e = juce::jlimit (s + 0.01, 1.0, endFrac);
    if (prepParams.startFrac == s && prepParams.endFrac == e) return;   // click that moved nothing
    recordEdit (tagTrim, "Trim");
    {
        const juce::ScopedLock sl (stateLock);
        prepParams.startFrac = s;
        prepParams.endFrac   = e;
    }
    requestRender();
    if (onChange) onChange();
}

// Each of these records its pre-change state first, tagged so a knob drag is one undo step.
// The no-op guards matter for undo as much as for CPU: a step that restores an identical state
// looks like a broken undo.
void AudioEngine::setFadeInMs (double ms)
{
    const double v = juce::jmax (0.0, ms);
    if (prepParams.fadeInMs == v) return;
    recordEdit (tagFadeIn, "Fade In");
    { const juce::ScopedLock sl (stateLock); prepParams.fadeInMs = v; } requestRender(); if (onChange) onChange();
}
void AudioEngine::setFadeOutMs (double ms)
{
    const double v = juce::jmax (0.0, ms);
    if (prepParams.fadeOutMs == v) return;
    recordEdit (tagFadeOut, "Fade Out");
    { const juce::ScopedLock sl (stateLock); prepParams.fadeOutMs = v; } requestRender(); if (onChange) onChange();
}
void AudioEngine::setOutFadeInMs (double ms)
{
    const double v = juce::jmax (0.0, ms);
    if (prepParams.outFadeInMs == v) return;
    recordEdit (tagOutFadeIn, "Output Fade In");
    { const juce::ScopedLock sl (stateLock); prepParams.outFadeInMs = v; } requestRender(); if (onChange) onChange();
}
void AudioEngine::setOutFadeOutMs (double ms)
{
    const double v = juce::jmax (0.0, ms);
    if (prepParams.outFadeOutMs == v) return;
    recordEdit (tagOutFadeOut, "Output Fade Out");
    { const juce::ScopedLock sl (stateLock); prepParams.outFadeOutMs = v; } requestRender(); if (onChange) onChange();
}
void AudioEngine::setGainDb (double db)
{
    if (prepParams.gainDb == db) return;
    recordEdit (tagGain, "Gain");
    { const juce::ScopedLock sl (stateLock); prepParams.gainDb = db; } requestRender(); if (onChange) onChange();
}
void AudioEngine::setNormalize (bool on)
{
    if (prepParams.normalize == on) return;
    recordEdit (tagNone, on ? "Normalize on" : "Normalize off");
    { const juce::ScopedLock sl (stateLock); prepParams.normalize = on; } requestRender(); if (onChange) onChange();
}

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

// The retained tail spans output samples [tailStart, tailStart + tailLen). Anything past the
// published length is the part we computed but cut off.
double AudioEngine::discardedTailSeconds() const
{
    if (fileSampleRate <= 0.0) return 0.0;
    const int end = tailStart.load() + tailLen.load();
    return juce::jmax (0, end - regionLen.load()) / fileSampleRate;
}

std::vector<float> AudioEngine::discardedTailPeaks (int numColumns) const
{
    // Lock-free on the message thread (as outputPeaks is): a concurrent publish can only make the
    // drawing one render stale, and swapping two live buffers never frees the one being read.
    std::vector<float> peaks ((size_t) juce::jmax (1, numColumns), 0.0f);
    const int off = regionLen.load() - tailStart.load();      // where the cut sits inside the buffer
    const int n   = tailLen.load() - juce::jmax (0, off);     // discarded samples
    if (numColumns <= 0 || n <= 0 || off < 0) return peaks;

    const int ch = tailBuffer.getNumChannels();
    for (int col = 0; col < numColumns; ++col)
    {
        const long long a = (long long) col * n / numColumns;
        const long long b = (long long) (col + 1) * n / numColumns;
        float peak = 0.0f;
        for (long long i = a; i < b && i < n; ++i)
            for (int c = 0; c < ch; ++c)
                peak = juce::jmax (peak, std::abs (tailBuffer.getSample (c, (int) (off + i))));
        peaks[(size_t) col] = juce::jmin (1.0f, peak);
    }
    return peaks;
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
