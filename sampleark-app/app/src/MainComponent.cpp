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
    variations.onMutate       = [this] { promptMutate(); };
    variations.onAddThis      = [this] { captureCurrent(); };
    variations.onRecall       = [this] (int i) { recallVariation (i); };
    variations.onToggleSelect = [this] (int i) { toggleFavourite (i); };
    variations.onToggleMute   = [this] (int i) { if (i >= 0 && i < (int) variationList.size()) { variationList[(size_t) i].muted = ! variationList[(size_t) i].muted; variations.repaint(); } };
    variations.onWrite        = [this] { writeSelected(); };
    variations.onPlayAll      = [this] { togglePlaylist(); };
    mutate.onChanged          = [this] { refreshMutateInfo(); };
    mutate.onBlueprintRecall  = [this] (int i) { recallBlueprint (i); };
    mutate.onBlueprintSave    = [this] (int i) { saveBlueprint (i); };
    refreshMutateInfo();   // seed the initial zone/% label

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
    if (showMutate) { mutate.setBounds (L.removeFromBottom (112)); L.removeFromBottom (10); }

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
    playlistActive = false; variations.setPlaylistActive (false);
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
    if (playlistActive) { playlistActive = false; variations.setPlaylistActive (false); }   // manual PLAY cancels the playlist
    engine.play();
    const double ms = intervalMsFor (playEveryId);
    if (ms > 0.0) retrigger.startTimer ((int) ms);
    else          retrigger.stopTimer();
}

void MainComponent::stopAll()
{
    // Halt transport + the retrigger. Loop (a lit toggle) clears; play-every (a dropdown
    // setting) stays armed, so PLAY/P resumes per-measure playback.
    if (playlistActive) { playlistActive = false; variations.setPlaylistActive (false); }
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

void MainComponent::refreshMutateInfo()
{
    const float lvl = mutate.level();
    variations.setMutateInfo (juce::String (depthZoneName (lvl)) + "  " + juce::String (juce::roundToInt (lvl * 100.0f)) + "%");
}

void MainComponent::saveBlueprint (int i)
{
    if (i < 0 || i >= (int) blueprints.size()) return;
    auto& b = blueprints[(size_t) i];
    b.filled = true;
    b.depth  = mutate.level();
    for (int k = 0; k < (int) Scope::Count; ++k) b.scope[(size_t) k] = mutate.scope (k);
    b.prep  = engine.prep();
    b.rack  = engine.rack();
    b.trans = engine.transformers();
    const juce::String tag = juce::String (depthZoneName (b.depth)).substring (0, 1);   // G/V/M/U
    mutate.setBlueprint (i, true, tag);
}

void MainComponent::recallBlueprint (int i)
{
    if (i < 0 || i >= (int) blueprints.size()) return;
    auto& b = blueprints[(size_t) i];
    if (! b.filled) return;
    mutate.setLevel (b.depth);
    bool flags[10]; for (int k = 0; k < 10; ++k) flags[k] = b.scope[(size_t) k];
    mutate.setScopeFlags (flags);
    engine.applyRecipe (b.prep, b.rack, b.trans);   // drop the saved sound into the live rack
    refreshMutateInfo();
}

int MainComponent::nextVariationNumber() const
{
    int mx = 0;
    for (const auto& v : variationList) if (! v.baseline) mx = juce::jmax (mx, v.number);
    return mx + 1;
}

static juce::String variationStem (AudioEngine& e)
{
    const auto stem = e.fileName().upToLastOccurrenceOf (".", false, false);
    return stem.isNotEmpty() ? stem : juce::String ("sample");
}

void MainComponent::captureCurrent()
{
    if (! engine.hasFile()) return;
    Variation v;
    v.prep   = engine.prep();
    v.rack   = engine.rack();
    v.trans  = engine.transformers();
    v.manual = true;
    v.number = nextVariationNumber();
    v.len    = engine.renderState (v.prep, v.rack, v.trans, engine.tempo(), v.audio);
    v.peaks  = peaksFromBuffer (v.audio, v.len, 96);
    v.name   = variationStem (engine) + "_var_" + juce::String (v.number).paddedLeft ('0', 2);
    v.selected = false;                          // sits in RAM until favourited
    variationList.push_back (std::move (v));
    variations.setActive ((int) variationList.size() - 1);   // it IS the current live rack
    variations.setStatus ("captured");
    variations.repaint();
}

void MainComponent::promptMutate()
{
    bool hasCandidates = false;
    for (const auto& v : variationList) if (! v.baseline) hasCandidates = true;
    if (! hasCandidates) { runMutate (true); return; }

    juce::PopupMenu m;
    m.addItem (1, "Add to existing");
    m.addItem (2, "Replace all");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&variations),
        [this] (int r) { if (r == 1) runMutate (false); else if (r == 2) runMutate (true); });
}

void MainComponent::runMutate (bool replace)
{
    if (! engine.hasFile()) return;

    Variation base;                          // current live settings are the mutation seed
    base.prep  = engine.prep();
    base.rack  = engine.rack();
    base.trans = engine.transformers();

    const float level = mutate.level();
    ScopeMask scope {};
    for (int i = 0; i < (int) Scope::Count; ++i) scope[(size_t) i] = mutate.scope (i);

    const juce::String base0 = variationStem (engine);
    constexpr int N = 16;

    if (replace) variationList.clear();

    // Ensure a Baseline (00) anchor exists at the front (created on the first batch).
    bool hasBaseline = false;
    for (const auto& v : variationList) if (v.baseline) hasBaseline = true;
    if (! hasBaseline)
    {
        Variation b = base;
        b.baseline = true; b.number = 0; b.selected = false;
        b.name = base0 + "_baseline";
        b.len  = engine.renderState (b.prep, b.rack, b.trans, engine.tempo(), b.audio);
        b.peaks = peaksFromBuffer (b.audio, b.len, 96);
        variationList.insert (variationList.begin(), std::move (b));
    }

    const int startNum = nextVariationNumber();
    for (int i = 0; i < N; ++i)
    {
        Variation v;
        const int num = startNum + i;
        sa::mutate (v, base, (juce::uint32) ((unsigned) num * 2654435761u + 1u), level, scope);
        v.number = num;
        v.len   = engine.renderState (v.prep, v.rack, v.trans, engine.tempo(), v.audio);
        v.peaks = peaksFromBuffer (v.audio, v.len, 96);
        v.name  = base0 + "_var_" + juce::String (num).paddedLeft ('0', 2);
        v.selected = replace && (i < kMaxFavourites);   // fresh batch pre-favourites 8; add-on selects none
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

void MainComponent::togglePlaylist()
{
    if (playlistActive)
    {
        playlistActive = false;
        variations.setPlaylistActive (false);
        engine.stop();
        topBar.refresh();
        return;
    }
    if (variationList.empty()) return;
    playlistActive = true;
    playlistIdx = -1;
    playlistWaitMs = 0.0;
    variations.setPlaylistActive (true);
    engine.setLoop (false);          // each variation plays once, then the gap, then the next
    playlistAdvance();               // kick off at the first non-muted entry
}

void MainComponent::playlistAdvance()
{
    const int n = (int) variationList.size();
    if (n == 0) { playlistActive = false; variations.setPlaylistActive (false); return; }
    for (int step = 1; step <= n; ++step)        // wrap around -> continuous loop until stopped
    {
        const int idx = (playlistIdx + step) % n;
        if (! variationList[(size_t) idx].muted)
        {
            playlistIdx = idx;
            playlistWaitMs = 0.0;
            engine.setLoop (false);
            recallVariation (idx);               // load + audition so the rack follows the playlist
            variations.setActive (idx);
            return;
        }
    }
    playlistActive = false;                       // everything muted
    variations.setPlaylistActive (false);
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
            playlistActive = false; variations.setPlaylistActive (false);
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
        // morph the FX graph in step with the modulation, and move the dynamics meters
        if (engine.isSlotModulated (engine.selectedSlot()) || engine.hasMeter (engine.selectedSlot()))
            detail.repaint();
    }
    else if (engine.hasFile() && ! engine.thumbnail().isFullyLoaded())
    {
        source.repaint();
    }

    // PLAY ALL playlist: when the current entry finishes, wait a ~1 s gap, then play the next.
    if (playlistActive)
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (engine.isPlaying())
            playlistWaitMs = 0.0;                         // still on the current entry
        else if (playlistWaitMs == 0.0)
            playlistWaitMs = nowMs + 1000.0;             // just finished -> schedule the gap
        else if (nowMs >= playlistWaitMs)
            playlistAdvance();                           // gap elapsed -> next
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
