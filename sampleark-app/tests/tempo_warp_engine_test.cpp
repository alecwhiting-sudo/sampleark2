// tempo_warp_engine_test.cpp — headless checks for M6 through the REAL AudioEngine render path:
// neutral pitch/tempo nulls, Transpose / Follow set the right lengths, the live worker render
// matches renderState (preview == print), DETECT is one undo step, Pitch mutation obeys its
// ranges and leaves tempo-locked loops alone. Exit code 0 = all pass. Run by CTest.
#include "AudioEngine.h"
#include "Variations.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

using namespace sa;

static int failures = 0, checks = 0;
static void check (const char* what, bool ok, double got = std::nan (""))
{
    ++checks;
    if (! ok) ++failures;
    if (std::isnan (got)) std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    else                  std::printf ("%s  %s  (%.4f)\n", ok ? "PASS" : "FAIL", what, got);
}

static constexpr double kSr = 44100.0;

// 4 bars of a simple beat at `bpm`: kicks on 1/3, snares on 2/4, hats on 8ths.
static juce::File makeLoop (double bpm)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sa_tempo_loop.wav");
    f.deleteFile();
    const double beat = 60.0 / bpm;
    const int n = (int) std::llround (16 * beat * kSr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random r (7);
    auto hit = [&] (double t, int kind)
    {
        const int s = (int) (t * kSr), len = (int) ((kind == 0 ? 0.25 : kind == 1 ? 0.18 : 0.04) * kSr);
        for (int i = 0; i < len && s + i < n; ++i)
        {
            const double tt = i / kSr;
            const double y = kind == 0 ? 0.8 * std::sin (2.0 * juce::MathConstants<double>::pi * (50.0 + 90.0 * std::exp (-tt * 30.0)) * tt) * std::exp (-tt * 12.0)
                           : kind == 1 ? 0.5 * (r.nextFloat() * 2.0 - 1.0) * std::exp (-tt * 22.0)
                                       : 0.2 * (r.nextFloat() * 2.0 - 1.0) * std::exp (-tt * 90.0);
            for (int c = 0; c < 2; ++c) b.addSample (c, s + i, (float) y);
        }
    };
    for (int bar = 0; bar < 4; ++bar)
    {
        const double t0 = bar * 4 * beat;
        hit (t0, 0); hit (t0 + 2 * beat, 0); hit (t0 + beat, 1); hit (t0 + 3 * beat, 1);
        for (int e = 0; e < 8; ++e) hit (t0 + e * 0.5 * beat, 2);
    }
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), kSr, 2, 32, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

static juce::AudioBuffer<float> readWav (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
    juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples);
    rd->read (&b, 0, (int) rd->lengthInSamples, 0, true, true);
    return b;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const double loopBpm = 136.0;
    auto file = makeLoop (loopBpm);
    const auto source = readWav (file);
    const int srcLen = source.getNumSamples();

    AudioEngine eng;
    if (! eng.loadFile (file)) { std::printf ("FAIL: could not load test file\n"); return 1; }
    auto settle = [&] { for (int i = 0; i < 500 && eng.isRenderBusy(); ++i) juce::Thread::sleep (10); juce::Thread::sleep (80); };
    settle();

    std::array<Transformer, kNumTransformers> trans {};
    FxRack dry;                                         // every slot bypassed: a neutral rack
    for (int s = 0; s < kNumSlots; ++s) if (dry.slots()[(size_t) s].on) dry.toggleBypass (s);
    juce::AudioBuffer<float> out;

    // --- neutral pitch/tempo is a null (only the edge fades touch the audio) ---
    {
        PrepParams p;
        const int len = eng.renderState (p, dry, trans, 120.0, out);
        double maxDiff = 0.0;
        for (int c = 0; c < 2; ++c)
            for (int i = 2000; i < srcLen - 2000; ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (out.getSample (c, i) - source.getSample (c, i)));
        check ("neutral PITCH + bypassed rack nulls against the source", len == srcLen && maxDiff == 0.0, maxDiff);
    }

    // --- transpose / follow lengths ---
    {
        PrepParams p; p.transposeSemis = 12.0;
        const int len = eng.renderState (p, dry, trans, 120.0, out);
        check ("Transpose +12 (Resample) halves the region", std::abs (len - srcLen / 2) <= 1, len);
        p = {}; p.loopBpm = loopBpm; p.followTempo = true;
        const int l2 = eng.renderState (p, dry, trans, 174.0, out);
        check ("Follow 136 -> 174 BPM conforms the length",
               std::abs (l2 - srcLen * loopBpm / 174.0) <= 1.0, l2);
        const int l3 = eng.renderState (p, dry, trans, loopBpm, out);
        check ("Follow at the loop's own tempo is untouched", l3 == srcLen, l3);
        p.followTempo = false;
        const int l4 = eng.renderState (p, dry, trans, 174.0, out);
        check ("Follow off ignores the session tempo", l4 == srcLen, l4);
    }

    // --- slowing down past the source length (buffers must grow) ---
    {
        PrepParams p; p.transposeSemis = -24.0;
        const int len = eng.renderState (p, dry, trans, 120.0, out);
        check ("Transpose -24 renders 4x the region", std::abs (len - srcLen * 4) <= 1, len);
    }

    // --- DETECT ---
    {
        const auto est = eng.detectRegionTempo();
        bool found = est.valid && std::abs (est.bpm - loopBpm) < 0.05;
        for (double a : est.alternatives) found = found || std::abs (a - loopBpm) < 0.05;
        check ("DETECT finds 136 BPM (primary or alternative)", found, est.bpm);
        check ("DETECT estimate is length-locked", est.lengthLocked);

        const int before = (int) eng.canUndo();
        eng.applyDetectedTempo (loopBpm);
        settle();
        check ("applyDetectedTempo sets the session tempo", eng.tempo() == loopBpm, eng.tempo());
        check ("applyDetectedTempo sets loop BPM + Follow",
               eng.prep().loopBpm == loopBpm && eng.prep().followTempo);
        check ("adopting the loop's tempo leaves the sound unchanged", eng.playbackRate() == 1.0, eng.playbackRate());
        check ("DETECT is undoable", eng.canUndo() && eng.undoName() == "Detect tempo");
        juce::ignoreUnused (before);

        eng.setTempo (174.0);
        settle();
        check ("raising the session tempo speeds the loop up (Follow)",
               std::abs (eng.lengthSeconds() - srcLen / kSr * loopBpm / 174.0) < 0.01, eng.lengthSeconds());
        check ("session tempo is not an undo step", eng.undoName() == "Detect tempo");

        eng.undo();
        settle();
        check ("undo DETECT clears loop BPM + Follow", eng.prep().loopBpm == 0.0 && ! eng.prep().followTempo);
        check ("undo DETECT leaves the session tempo alone", eng.tempo() == 174.0, eng.tempo());
        check ("after undo the region plays at its own speed again",
               std::abs (eng.lengthSeconds() - srcLen / kSr) < 0.01, eng.lengthSeconds());
        eng.setTempo (120.0);
    }

    // --- preview == print: the live worker render matches renderState sample for sample ---
    {
        eng.setTransposeSemis (5.0);
        eng.setTuneCents (-23.0);
        settle();
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sa_tempo_live.wav");
        const bool ok = eng.exportPreppedTo (tmp, 32);
        const auto live = readWav (tmp);
        const int len = eng.renderState (eng.prep(), eng.rack(), eng.transformers(), eng.tempo(), out);
        double maxDiff = ok && live.getNumSamples() == len ? 0.0 : 1.0;
        for (int c = 0; c < 2 && maxDiff == 0.0; ++c)
            for (int i = 0; i < len; ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (live.getSample (c, i) - out.getSample (c, i)));
        check ("live render == renderState with pitch active (preview == print)", maxDiff == 0.0, maxDiff);

        juce::AudioBuffer<float> again;
        const int len2 = eng.renderState (eng.prep(), eng.rack(), eng.transformers(), eng.tempo(), again);
        bool same = len2 == len;
        for (int c = 0; c < 2 && same; ++c)
            for (int i = 0; i < len && same; ++i) same = again.getSample (c, i) == out.getSample (c, i);
        check ("pitched render is deterministic", same);
        check ("Transpose/Tune are undoable", eng.undoName() == "Tune");
        tmp.deleteFile();
    }

    // --- Pitch mutation ---
    {
        ScopeMask pitchOnly {}; pitchOnly[(size_t) Scope::Pitch] = true;
        Variation base; base.prep = PrepParams{};
        double maxCents = 0.0, maxSemis = 0.0;
        for (juce::uint32 seed = 1; seed <= 60; ++seed)
        {
            Variation v; mutate (v, base, seed, 0.2f, pitchOnly);
            maxCents = juce::jmax (maxCents, std::abs (v.prep.tuneCents));
            maxSemis = juce::jmax (maxSemis, std::abs (v.prep.transposeSemis));
        }
        check ("Gentle pitch stays within ±10 cents, no transposition", maxCents <= 10.0 && maxSemis == 0.0, maxCents);

        bool octave = false; double vibSemis = 0.0;
        for (juce::uint32 seed = 1; seed <= 60; ++seed)
        {
            Variation v; mutate (v, base, seed, 0.4f, pitchOnly);
            vibSemis = juce::jmax (vibSemis, std::abs (v.prep.transposeSemis));
            Variation u; mutate (u, base, seed, 0.95f, pitchOnly);
            octave = octave || std::abs (u.prep.transposeSemis) >= 12.0;
        }
        check ("Vibing transposes at most ±1 semitone", vibSemis <= 1.0, vibSemis);
        check ("Unsafe reaches octave jumps", octave);

        base.prep.loopBpm = 136.0; base.prep.followTempo = true;
        bool untouched = true;
        for (juce::uint32 seed = 1; seed <= 30; ++seed)
        {
            Variation v; mutate (v, base, seed, 0.9f, pitchOnly);
            untouched = untouched && v.prep.transposeSemis == 0.0 && v.prep.tuneCents == 0.0;
        }
        check ("a tempo-locked Resample loop is never detuned by mutation", untouched);
    }

    file.deleteFile();
    std::printf ("\n%d / %d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
