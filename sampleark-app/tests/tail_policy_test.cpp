// tail_policy_test.cpp — headless checks for the Delay/Reverb tail policy (Arch06 -> TAIL row).
//
// Drives the REAL render path (AudioEngine::renderState plus the live worker render), not a
// reimplementation: all four tail modes, the reverb/delay interaction, exact-length padding,
// the retained-tail capture, and mutation inheritance. 13 assertions; exit code 0 = all pass.
//
// NOT wired into CMake yet (Arch07 P4 covers promoting it to a real target). Until then it
// builds by borrowing the app target's own flags and objects, after a normal app build:
//
//   D=build-native/app/CMakeFiles/SampleArk.dir
//   DEFS=$(grep '^CXX_DEFINES = ' $D/flags.make | sed 's/^CXX_DEFINES = //')
//   INCS=$(grep '^CXX_INCLUDES = ' $D/flags.make | sed 's/^CXX_INCLUDES = //')
//   c++ ${=DEFS} ${=INCS} -g -std=gnu++17 -arch arm64 -O0 -w -Iapp/src \
//       -c tests/tail_policy_test.cpp -o /tmp/tail_policy_test.o
//   # then link with link.txt, minus Main.cpp.o (the app entry point), plus the object above
//
// (zsh needs ${=VAR} so the flag strings word-split.)

// Headless check of the tail policy: drives the real render path (AudioEngine::renderState and
// the live worker render) and prints the published lengths.
#include "AudioEngine.h"
#include "Variations.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

using namespace sa;

static const int kSlotDelay = 3, kSlotReverb = 4;

static juce::File makeTestWav (double secs, double sr)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sa_tailtest.wav");
    f.deleteFile();
    const int n = (int) (secs * sr);
    juce::AudioBuffer<float> b (2, n);
    juce::Random r (1);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)                       // decaying noise burst, silent tail-free
            b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * 0.6f * std::exp (-3.0f * (float) i / (float) n));
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

static void setTail (FxRack& rack, int slot, TailMode m, float amt = 0.5f)
{
    rack.setTailMode (slot, m);
    rack.setTailAmount (slot, amt);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const double sr = 44100.0, tempo = 120.0;
    auto file = makeTestWav (1.0, sr);

    AudioEngine eng;
    if (! eng.loadFile (file)) { printf ("FAIL: could not load test file\n"); return 1; }

    PrepParams prep;                                   // whole file = 1.000 s region
    std::array<Transformer, kNumTransformers> trans {};
    juce::AudioBuffer<float> out;

    auto run = [&] (const char* label, TailMode rvbMode, float rvbAmt, TailMode dlyMode, float dlyAmt)
    {
        FxRack rack;                                   // defaults: delay + reverb both active
        rack.setParam (kSlotDelay, 0, 0.0536f);        // ~54 ms free time -> ~0.30 s ring-out
        rack.setParam (kSlotDelay, 1, 0.30f);          // feedback
        rack.setParam (kSlotReverb, 0, 0.0f);          // size 0 + decay 0 -> 0.60 s ring-out
        rack.setParam (kSlotReverb, 1, 0.0f);
        setTail (rack, kSlotReverb, rvbMode, rvbAmt);
        setTail (rack, kSlotDelay,  dlyMode, dlyAmt);

        const double natRvb = naturalTailSeconds (rack.slots()[kSlotReverb], tempo);
        const double natDly = naturalTailSeconds (rack.slots()[kSlotDelay], tempo);
        const int len = eng.renderState (prep, rack, trans, tempo, out);
        printf ("%-34s natural(rvb %.2f dly %.2f)  render window %.3f s  ->  OUT %.3f s\n",
                label, natRvb, natDly, rack.naturalTail (tempo) + 1.0, len / sr);
        return len / sr;
    };

    printf ("region = 1.000 s   (reverb rings 0.60 s, delay rings 0.30 s)\n\n");
    const double bothFull = run ("both FULL",              TailMode::Full, 0, TailMode::Full, 0);
    const double rvbOff   = run ("reverb OFF, delay FULL", TailMode::Off,  0, TailMode::Full, 0);
    const double dlyOff   = run ("reverb FULL, delay OFF", TailMode::Full, 0, TailMode::Off,  0);
    const double bothOff  = run ("both OFF",               TailMode::Off,  0, TailMode::Off,  0);
    const double timed    = run ("delay TIME 250ms, rvb OFF", TailMode::Off, 0, TailMode::Time, 0.466f);
    const double mult     = run ("reverb xLOOP 1.0, dly OFF", TailMode::Mult, 0.6836f, TailMode::Off, 0);
    const double longTime = run ("delay TIME 2s, rvb OFF",  TailMode::Off,  0, TailMode::Time, 0.76701f);

    auto check = [] (const char* what, double got, double want, double tol)
    {
        const bool ok = std::abs (got - want) <= tol;
        printf ("%-28s want %.3f  got %.3f   %s\n", what, want, got, ok ? "PASS" : "*** FAIL ***");
        return ok;
    };
    printf ("\n");
    bool ok = true;
    ok &= check ("both FULL = region+reverb",  bothFull, 1.60, 0.06);
    ok &= check ("reverb off -> delay only",   rvbOff,   1.30, 0.06);
    ok &= check ("delay off -> reverb only",   dlyOff,   1.60, 0.06);
    ok &= check ("both off = region exactly",  bothOff,  1.00, 0.0005);
    ok &= check ("TIME 250 ms exact",          timed,    1.25, 0.0005);
    ok &= check ("xLOOP 1.0 exact",            mult,     2.00, 0.0005);
    ok &= check ("TIME 2 s padded exact",      longTime, 3.00, 0.0015);

    // Live path: the worker render + the retained-tail capture behind the cut.
    eng.rackSetParam (kSlotDelay, 0, 0.0536f); eng.rackSetParam (kSlotDelay, 1, 0.30f);
    eng.rackSetParam (kSlotReverb, 0, 0.0f);   eng.rackSetParam (kSlotReverb, 1, 0.0f);
    auto settle = [&] { for (int i = 0; i < 400 && eng.isRenderBusy(); ++i) juce::Thread::sleep (10); juce::Thread::sleep (60); };
    settle();
    printf ("\nlive render (worker + retained tail)\n");
    eng.rackSetTailMode (kSlotReverb, TailMode::Full); eng.rackSetTailMode (kSlotDelay, TailMode::Full); settle();
    printf ("  both FULL : out %.3f s   retained-but-cut %.3f s\n", eng.lengthSeconds(), eng.discardedTailSeconds());
    const double fullOut = eng.lengthSeconds();
    eng.rackSetTailMode (kSlotReverb, TailMode::Off); settle();
    printf ("  rvb OFF   : out %.3f s   retained-but-cut %.3f s\n", eng.lengthSeconds(), eng.discardedTailSeconds());
    const double rvbOffOut = eng.lengthSeconds(), rvbOffCut = eng.discardedTailSeconds();
    eng.rackSetTailMode (kSlotDelay, TailMode::Off); settle();
    printf ("  both OFF  : out %.3f s   retained-but-cut %.3f s\n", eng.lengthSeconds(), eng.discardedTailSeconds());
    const double bothOffOut = eng.lengthSeconds(), bothOffCut = eng.discardedTailSeconds();

    ok &= check ("live both FULL",             fullOut,    1.60, 0.06);
    ok &= check ("live rvb OFF",               rvbOffOut,  1.30, 0.06);
    ok &= check ("live both OFF",              bothOffOut, 1.00, 0.0005);
    ok &= check ("cut tail kept (rvb off)",    rvbOffCut > 0.1 ? 1.0 : 0.0, 1.0, 0.0001);
    ok &= check ("cut tail kept (both off)",   bothOffCut > 0.3 ? 1.0 : 0.0, 1.0, 0.0001);

    // Mutation must inherit the tail policy untouched.
    Variation base; base.rack = FxRack{};
    setTail (base.rack, kSlotReverb, TailMode::Off);
    setTail (base.rack, kSlotDelay,  TailMode::Mult, 0.6836f);
    ScopeMask all {}; all[(size_t) Scope::Everything] = true;
    bool inherited = true;
    for (int i = 0; i < 40; ++i)
    {
        Variation v; mutate (v, base, (juce::uint32) (i + 1), 1.0f, all);   // Unsafe
        // Unsafe mutation reorders the rack, so find the effects by type — the tail policy
        // travels with its effect rather than staying on a slot index.
        const FxSlot* rvb = nullptr; const FxSlot* dly = nullptr;
        for (const auto& s : v.rack.slots())
        {
            if (s.type == FxType::Reverb) rvb = &s;
            if (s.type == FxType::Delay)  dly = &s;
        }
        inherited &= rvb != nullptr && dly != nullptr
                  && rvb->tailMode == TailMode::Off
                  && dly->tailMode == TailMode::Mult
                  && std::abs (dly->tailAmount - 0.6836f) < 1.0e-6f;
    }
    ok &= check ("mutation inherits tail",     inherited ? 1.0 : 0.0, 1.0, 0.0001);

    printf ("\n%s\n", ok ? "ALL PASS" : "FAILURES ABOVE");
    file.deleteFile();
    return ok ? 0 : 1;
}
