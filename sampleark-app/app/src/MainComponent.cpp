#include "MainComponent.h"
#include "Theme.h"
#include <cmath>

namespace sa
{
MainComponent::MainComponent()
{
    topBar.setEngine (&engine);
    source.setEngine (&engine);
    prep.setEngine (&engine);
    rack.setEngine (&engine);
    detail.setEngine (&engine);
    transformers.setEngine (&engine);

    addAndMakeVisible (topBar);
    addAndMakeVisible (source);
    addAndMakeVisible (transformers);
    addAndMakeVisible (prep);
    addAndMakeVisible (rack);
    addAndMakeVisible (detail);
    addAndMakeVisible (mutate);
    addAndMakeVisible (variations);
    addAndMakeVisible (inputs);

    inputs.onLoadFile = [this] (const juce::File& f) { loadFile (f); };

    variations.setData (&variationList);
    variations.onMutate       = [this] { generateVariations(); };
    variations.onRecall       = [this] (int i) { recallVariation (i); };
    variations.onToggleSelect = [this] (int i) { toggleFavourite (i); };
    variations.onToggleMute   = [this] (int i) { if (i >= 0 && i < (int) variationList.size()) { variationList[(size_t) i].muted = ! variationList[(size_t) i].muted; variations.repaint(); } };
    variations.onWrite        = [this] { writeSelected(); };
    variations.onKeepPlaying  = [this] { if (lastAuditioned >= 0) { engine.setLoop (true); auditionVariation (lastAuditioned); } };
    mutate.onChanged          = [this] { variations.repaint(); };

    topBar.onView = [this] (int zone) { toggleZone (zone); };

    transformers.onOverlayToggle = [this] (bool on)
    {
        source.setOverlayTransformer (on ? transformers.activeIndex() : -1);
        if (on) source.ensureOutputVisible();
        resized();
        source.repaint();
    };
    transformers.onActiveChanged = [this] (int i)
    {
        if (transformers.isOverlay()) source.setOverlayTransformer (i);
    };

    topBar.onPlay = [this] { startPlayback(); topBar.refresh(); };
    topBar.onStop = [this] { stopAll(); };
    topBar.onLoad = [this] { openChooser(); };
    topBar.onLoop = [this] { engine.setLoop (! engine.isLoopOn()); };
    topBar.onEvery = [this] (int id) { setPlayEvery (id); };
    engine.onChange = [this] { topBar.refresh(); source.repaint(); prep.refresh(); rack.repaint(); detail.refresh(); transformers.refresh(); };

    retrigger.onTick = [this] { engine.play(); };

    setWantsKeyboardFocus (true);
    startTimerHz (30);
    setSize (1380, 860);
    applyVisibility();
    source.setOverlayTransformer (transformers.isOverlay() ? transformers.activeIndex() : -1);
}

void MainComponent::toggleZone (int zone)
{
    switch (zone)
    {
        case 0: showSample = ! showSample; break;
        case 1: showTrans  = ! showTrans;  break;
        case 2: showFx     = ! showFx;     break;
        case 3: showMutate = ! showMutate; break;
        case 4: showVars   = ! showVars;   break;
        case 5: showInputs = ! showInputs; break;
        default: return;
    }
    applyVisibility();
}

void MainComponent::applyVisibility()
{
    source.setVisible (showSample);
    transformers.setVisible (showTrans);
    rack.setVisible (showFx);
    detail.setVisible (showFx);
    mutate.setVisible (showMutate);
    variations.setVisible (showVars);
    inputs.setVisible (showInputs);

    topBar.setViewLit (0, showSample);
    topBar.setViewLit (1, showTrans);
    topBar.setViewLit (2, showFx);
    topBar.setViewLit (3, showMutate);
    topBar.setViewLit (4, showVars);
    topBar.setViewLit (5, showInputs);
    topBar.repaint();
    resized();
}

MainComponent::~MainComponent()
{
    retrigger.stopTimer();
    stopTimer();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (sa::theme::colour::bg);
}

void MainComponent::resized()
{
    using namespace sa::theme;
    auto area = getLocalBounds();
    topBar.setBounds (area.removeFromTop (dim::toolbarH));

    // Three columns: INPUTS (far left) | main stack (centre) | VARIATIONS (far right).
    // A hidden side gives its width back to the centre stack.
    auto body = area;
    const int totalW = body.getWidth();
    if (showInputs)
        inputs.setBounds (body.removeFromLeft (juce::roundToInt (totalW * 0.20f)));
    if (showVars)
        variations.setBounds (body.removeFromRight (juce::roundToInt (totalW * (1.0f - dim::leftFrac))));
    auto left = body;

    auto L = left.reduced (12, 12);

    // MUTATE strip pinned to the bottom.
    if (showMutate) { mutate.setBounds (L.removeFromBottom (88)); L.removeFromBottom (10); }

    // Top-to-bottom: SAMPLE, TRANSFORMERS, PREP (always), FX (rack + detail).
    // FX is the primary flexible filler; if FX is hidden, SAMPLE expands to fill instead.
    const int transH = transformers.isOverlay() ? 118 : 210, prepH = 100, sampleDefault = 150;
    int fixedBelowSample = (showTrans ? transH + 10 : 0) + prepH + 10;
    int rem = L.getHeight() - fixedBelowSample;

    int sampleH = 0, fxH = 0;
    if (showSample && showFx)      { sampleH = sampleDefault; fxH = juce::jmax (0, rem - sampleDefault - 10); }
    else if (showSample)           { sampleH = juce::jmax (0, rem); }
    else if (showFx)               { fxH = juce::jmax (0, rem - 10); }

    if (showSample) { source.setBounds (L.removeFromTop (sampleH)); L.removeFromTop (10); }
    if (showTrans)  { transformers.setBounds (L.removeFromTop (transH)); L.removeFromTop (10); }
    prep.setBounds (L.removeFromTop (prepH));
    L.removeFromTop (10);
    if (showFx)
    {
        auto middle = L.removeFromTop (fxH);
        rack.setBounds (middle.removeFromLeft (juce::roundToInt (middle.getWidth() * 0.43f)));
        middle.removeFromLeft (10);
        detail.setBounds (middle);
    }
}

void MainComponent::loadFile (const juce::File& file)
{
    engine.loadFile (file);
    inputs.setLoaded (file);   // highlight it in the browser
    topBar.repaint();
    source.repaint();

    // a fresh source invalidates any previous batch — clear until the user hits MUTATE
    variationList.clear();
    lastAuditioned = -1;
    variations.setActive (-1);
    variations.setStatus ("Hit MUTATE to generate");
    variations.repaint();
}

void MainComponent::openChooser()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load a sample",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile())
            loadFile (f);
    });
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (showInputs && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
    {
        inputs.step (key == juce::KeyPress::downKey ? +1 : -1);   // audition next/prev input
        return true;
    }
    const int code = key.getKeyCode();   // letters report uppercase regardless of caps/shift
    if (code == 'P' && ! key.getModifiers().isCommandDown())
    {
        startPlayback();      // P = (re)start, honouring the armed play-every mode
        topBar.refresh();
        return true;
    }
    if (code == 'S' && ! key.getModifiers().isCommandDown())
    {
        stopAll();            // S = stop loop / play-every / playback
        return true;
    }
    if (key == juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0))
    {
        exportPrepped();
        return true;
    }
    return false;
}

double MainComponent::intervalMsFor (int comboId) const
{
    const double beatMs = 60000.0 / engine.tempo();
    if (comboId == 2) return beatMs;        // 1/4 note
    if (comboId == 3) return beatMs * 2.0;  // 1/2 note
    if (comboId == 4) return beatMs * 4.0;  // bar (4/4)
    return 0.0;                             // Off
}

void MainComponent::setPlayEvery (int comboId)
{
    playEveryId = comboId;                  // persists across STOP
    if (intervalMsFor (comboId) > 0.0)
        startPlayback();
    else
        retrigger.stopTimer();
}

void MainComponent::startPlayback()
{
    engine.play();
    const double ms = intervalMsFor (playEveryId);
    if (ms > 0.0) retrigger.startTimer ((int) ms);
    else          retrigger.stopTimer();
}

void MainComponent::stopAll()
{
    // Halt transport + the retrigger. Loop (a lit toggle) clears; play-every (a dropdown
    // setting) stays armed, so PLAY/P resumes per-measure playback.
    retrigger.stopTimer();
    engine.setLoop (false);
    engine.stop();
    source.repaint();
    topBar.refresh();
}

void MainComponent::exportPrepped()
{
    if (! engine.hasFile())
        return;

    const auto stem = engine.fileName().upToLastOccurrenceOf (".", false, false);
    auto suggested = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                         .getChildFile (stem + "_prepped.wav");

    chooser = std::make_unique<juce::FileChooser> ("Export prepped one-shot (24-bit WAV)",
                                                   suggested, "*.wav");
    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::warnAboutOverwriting;
    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f != juce::File())
            engine.exportPreppedTo (f, 24);
    });
}

// ---- M4 variations ----
static std::vector<float> peaksFromBuffer (const juce::AudioBuffer<float>& buf, int len, int cols)
{
    std::vector<float> pk ((size_t) juce::jmax (1, cols), 0.0f);
    const int n = juce::jmin (len, buf.getNumSamples());
    const int ch = buf.getNumChannels();
    if (n <= 0 || ch <= 0) return pk;
    for (int c = 0; c < cols; ++c)
    {
        const long long a = (long long) c * n / cols;
        const long long b = (long long) (c + 1) * n / cols;
        float peak = 0.0f;
        for (long long i = a; i < b && i < n; ++i)
            for (int ch2 = 0; ch2 < ch; ++ch2)
                peak = juce::jmax (peak, std::abs (buf.getSample (ch2, (int) i)));
        pk[(size_t) c] = juce::jmin (1.0f, peak);
    }
    return pk;
}

void MainComponent::generateVariations()
{
    if (! engine.hasFile()) return;

    Variation base;                          // current working state is the seed
    base.prep  = engine.prep();
    base.rack  = engine.rack();
    base.trans = engine.transformers();

    const float level = mutate.level();
    ScopeMask scope {};
    for (int i = 0; i < (int) Scope::Count; ++i) scope[(size_t) i] = mutate.scope (i);

    const juce::String stem = engine.fileName().upToLastOccurrenceOf (".", false, false);
    const juce::String base0 = stem.isNotEmpty() ? stem : juce::String ("sample");
    constexpr int N = 16;

    variationList.clear();
    variationList.reserve ((size_t) N + 1);

    // Row 0 = Baseline: the unvaried state the user set, rendered as-is. The recall anchor / undo.
    {
        Variation b = base;
        b.baseline = true;
        b.selected = false;
        b.name     = base0 + "_baseline";
        b.len      = engine.renderState (b.prep, b.rack, b.trans, engine.tempo(), b.audio);
        b.peaks    = peaksFromBuffer (b.audio, b.len, 96);
        variationList.push_back (std::move (b));
    }

    for (int i = 0; i < N; ++i)
    {
        Variation v;
        sa::mutate (v, base, (juce::uint32) (i * 2654435761u + 1u), level, scope);
        v.len   = engine.renderState (v.prep, v.rack, v.trans, engine.tempo(), v.audio);
        v.peaks = peaksFromBuffer (v.audio, v.len, 96);
        v.name  = base0 + "_var_" + juce::String (i + 1).paddedLeft ('0', 2);
        v.selected = (i < kMaxFavourites);   // first 8 favourited by default (also the max)
        variationList.push_back (std::move (v));
    }
    lastAuditioned = -1;
    variations.setActive (0);                // live rack == Baseline right after generating
    variations.setStatus ({});
    variations.repaint();
}

void MainComponent::recallVariation (int i)
{
    if (i < 0 || i >= (int) variationList.size()) return;
    auto& v = variationList[(size_t) i];
    engine.applyRecipe (v.prep, v.rack, v.trans);   // drop the recipe into the live rack + transformers (re-renders)
    variations.setActive (i);
    auditionVariation (i);                          // instant audio from the stored buffer + show in OUTPUT
}

void MainComponent::toggleFavourite (int i)
{
    if (i < 0 || i >= (int) variationList.size()) return;
    auto& v = variationList[(size_t) i];
    if (! v.selected)
    {
        int count = 0; for (auto& x : variationList) if (x.selected) ++count;
        if (count >= kMaxFavourites)                // hard cap — refuse and flag it
        {
            variations.setStatus (juce::String (kMaxFavourites) + " favourites max");
            return;
        }
    }
    v.selected = ! v.selected;
    variations.setStatus ({});
    variations.repaint();
}

void MainComponent::auditionVariation (int i)
{
    if (i < 0 || i >= (int) variationList.size()) return;
    lastAuditioned = i;
    auto& v = variationList[(size_t) i];
    if (v.muted) return;
    engine.auditionBuffer (v.audio, v.len);
    topBar.refresh();
    source.repaint();
}

void MainComponent::writeSelected()
{
    int count = 0; for (auto& v : variationList) if (v.selected) ++count;
    if (count == 0) return;

    chooser = std::make_unique<juce::FileChooser> ("Choose a folder to write the selected variations",
                  juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (! dir.isDirectory()) return;
            juce::StringArray manifest;
            manifest.add ("# SampleArk variations");
            int written = 0;
            for (auto& v : variationList)
            {
                if (! v.selected) continue;
                auto f = dir.getChildFile (v.name + ".wav").getNonexistentSibling();
                if (engine.writeWav (f, v.audio, v.len, 24)) { ++written; manifest.add (f.getFileName()); }
            }
            dir.getChildFile ("manifest.txt").replaceWithText (manifest.joinIntoString ("\n"));

            // Once written, the files on disk are the only memory — clear the RAM stack.
            variationList.clear();
            lastAuditioned = -1;
            variations.setActive (-1);
            variations.setStatus (juce::String (written) + " written -> stack cleared");
            variations.repaint();
        });
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff"))
            return true;
    return false;
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.existsAsFile())
        {
            loadFile (file);
            break;
        }
    }
}

void MainComponent::timerCallback()
{
    // Take keyboard focus once we're on screen, so P/S work from launch (no click needed).
    if (! focusGrabbed && isShowing())
    {
        grabKeyboardFocus();
        focusGrabbed = true;
    }

    // Advance the playhead while playing; also catch the async thumbnail finishing.
    if (engine.isPlaying())
    {
        // One-shot: stop at the region end so the playhead doesn't drift into trailing silence.
        if (! engine.isLoopOn() && engine.lengthSeconds() > 0.0
            && engine.positionSeconds() >= engine.lengthSeconds())
        {
            engine.stop();
            topBar.refresh();
        }
        source.repaint();
        transformers.repaint();   // move the transformer playhead while playing
    }
    else if (engine.hasFile() && ! engine.thumbnail().isFullyLoaded())
    {
        source.repaint();
    }

    // Repaint the SAMPLE panel while a background render runs and when it completes.
    const int ver = engine.renderVersion();
    if (engine.isRenderBusy() || ver != lastRenderVer)
    {
        lastRenderVer = ver;
        source.repaint();
    }
}
}
