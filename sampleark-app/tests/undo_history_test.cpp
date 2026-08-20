// undo_history_test.cpp — headless checks for the edit history (Arch06 -> UNDO/REDO).
//
// Drives the real AudioEngine setters, so it covers what the UI actually calls: gesture
// coalescing, no-op rejection, redo invalidation, the step cap, and state restoration across
// a rack reorder. Exit code 0 = all pass. Build it the same way as tail_policy_test.cpp.

#include "AudioEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

using namespace sa;

static int failures = 0;

static void check (const char* what, bool ok)
{
    printf ("  %-46s %s\n", what, ok ? "PASS" : "*** FAIL ***");
    if (! ok) ++failures;
}

static juce::File makeTestWav (double secs, double sr)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("sa_undotest.wav");
    f.deleteFile();
    const int n = (int) (secs * sr);
    juce::AudioBuffer<float> b (2, n);
    juce::Random r (7);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
            b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * 0.4f);
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, n);
    return f;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto file = makeTestWav (0.5, 44100.0);

    AudioEngine eng;
    if (! eng.loadFile (file)) { printf ("FAIL: could not load test file\n"); return 1; }

    constexpr int kFilter = 5;   // default rack layout: slot 5 = Filter

    printf ("basics\n");
    check ("nothing to undo on a fresh load", ! eng.canUndo() && ! eng.canRedo());
    eng.setGainDb (6.0);
    check ("an edit becomes an undo step", eng.canUndo());
    check ("step is named", eng.undoName() == "Gain");
    check ("undo restores the old value", eng.undo() && eng.prep().gainDb == 0.0);
    check ("undo leaves a redo", eng.canRedo() && ! eng.canUndo());
    check ("redo re-applies", eng.redo() && eng.prep().gainDb == 6.0);
    check ("redo empties the redo stack", ! eng.canRedo() && eng.canUndo());
    check ("undo bottoms out cleanly", eng.undo() && ! eng.undo());
    check ("gain is back to the start", eng.prep().gainDb == 0.0);

    printf ("gesture coalescing\n");
    while (eng.canUndo()) eng.undo();
    const float base = eng.rack().slots()[kFilter].params[0];
    for (int i = 1; i <= 60; ++i)                              // one continuous knob drag
        eng.rackSetParam (kFilter, 0, base + (float) i * 0.002f);
    check ("60 rapid moves = one step", eng.undoName() == "Filter Cutoff");
    eng.undo();
    check ("undo returns to the pre-drag value", std::abs (eng.rack().slots()[kFilter].params[0] - base) < 1.0e-6f);
    check ("and there is nothing left behind it", ! eng.canUndo());

    printf ("gesture separation\n");
    while (eng.canUndo()) eng.undo();
    eng.rackSetParam (kFilter, 0, 0.30f);
    eng.rackSetParam (kFilter, 1, 0.40f);                      // different param = its own gesture
    check ("a different target starts a new step", eng.undoName() == "Filter Reso");
    eng.undo();
    check ("first undo restores reso only", eng.rack().slots()[kFilter].params[1] != 0.40f
                                         && eng.rack().slots()[kFilter].params[0] == 0.30f);
    eng.undo();
    check ("second undo restores cutoff", eng.rack().slots()[kFilter].params[0] != 0.30f);

    printf ("no-op rejection\n");
    while (eng.canUndo()) eng.undo();
    const float held = eng.rack().slots()[kFilter].params[0];
    for (int i = 0; i < 20; ++i) eng.rackSetParam (kFilter, 0, held);   // drag that never moves
    check ("setting the same value records nothing", ! eng.canUndo());
    const double gain = eng.prep().gainDb;
    eng.setGainDb (gain);
    check ("same for prep params", ! eng.canUndo());

    printf ("redo invalidation\n");
    while (eng.canUndo()) eng.undo();
    eng.setGainDb (3.0);
    eng.undo();
    check ("redo available after undo", eng.canRedo());
    eng.setNormalize (true);                                   // a new edit abandons the redo chain
    check ("a fresh edit clears redo", ! eng.canRedo());

    printf ("structural edits\n");
    while (eng.canUndo()) eng.undo();
    const auto typeAt = [&eng] (int s) { return (int) eng.rack().slots()[(size_t) s].type; };
    const int t3 = typeAt (3), t4 = typeAt (4);
    eng.rackMove (3, 4);
    check ("reorder is one step", eng.canUndo() && typeAt (4) == t3);
    eng.undo();
    check ("undo restores rack order", typeAt (3) == t3 && typeAt (4) == t4);
    const bool wasOn = eng.rack().slots()[kFilter].on;
    eng.rackToggleBypass (kFilter);
    eng.undo();
    check ("undo restores bypass", eng.rack().slots()[kFilter].on == wasOn);
    eng.rackSetTailMode (4, TailMode::Full);
    check ("tail mode change is a step", eng.undoName().contains ("Tail"));
    eng.undo();
    check ("undo restores tail mode", eng.rack().slots()[4].tailMode == TailMode::Off);   // Off is the default
    eng.rackSetTailMode (4, TailMode::Off);
    check ("setting the mode it already has records nothing", ! eng.canUndo());

    printf ("recall + transformers\n");
    while (eng.canUndo()) eng.undo();
    {
        auto prep = eng.prep(); auto rack = eng.rack(); auto trans = eng.transformers();
        rack.setParam (kFilter, 0, 0.123f);
        eng.applyRecipe (prep, rack, trans);                   // clicking a variation row
        check ("recall is undoable", eng.undoName() == "Recall");
        eng.undo();
        check ("undo restores the hand-built rack", eng.rack().slots()[kFilter].params[0] != 0.123f);
    }
    {
        auto t = eng.transformers()[0];
        t.on = true; t.depth = 0.9f;
        eng.setTransformer (0, t);
        check ("transformer edit is a step", eng.undoName() == "Transformer 1");
        eng.undo();
        check ("undo restores the transformer", ! eng.transformers()[0].on);
    }

    printf ("history cap + file load\n");
    while (eng.canUndo()) eng.undo();
    for (int i = 0; i < 150; ++i) eng.rackToggleBypass (kFilter);   // 150 discrete steps
    int steps = 0;
    while (eng.canUndo() && steps < 500) { eng.undo(); ++steps; }
    check ("history is capped at 100 steps", steps == 100);
    eng.setGainDb (9.0);
    check ("edits exist before reload", eng.canUndo());
    eng.loadFile (file);
    check ("loading a sample clears history", ! eng.canUndo() && ! eng.canRedo());

    printf ("\n%s\n", failures == 0 ? "ALL PASS" : "FAILURES ABOVE");
    file.deleteFile();
    return failures == 0 ? 0 : 1;
}
