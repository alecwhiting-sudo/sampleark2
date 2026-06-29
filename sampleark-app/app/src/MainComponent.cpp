#include "MainComponent.h"
#include "Theme.h"

namespace sa
{
MainComponent::MainComponent()
{
    topBar.setEngine (&engine);
    source.setEngine (&engine);
    prep.setEngine (&engine);
    rack.setEngine (&engine);
    detail.setEngine (&engine);

    addAndMakeVisible (topBar);
    addAndMakeVisible (source);
    addAndMakeVisible (transformers);
    addAndMakeVisible (prep);
    addAndMakeVisible (rack);
    addAndMakeVisible (detail);
    addAndMakeVisible (mutate);
    addAndMakeVisible (variations);

    topBar.onView = [this] (int zone) { toggleZone (zone); };

    topBar.onPlay = [this] { startPlayback(); topBar.refresh(); };
    topBar.onStop = [this] { stopAll(); };
    topBar.onLoad = [this] { openChooser(); };
    topBar.onLoop = [this] { engine.setLoop (! engine.isLoopOn()); };
    topBar.onEvery = [this] (int id) { setPlayEvery (id); };
    engine.onChange = [this] { topBar.refresh(); source.repaint(); prep.refresh(); rack.repaint(); detail.refresh(); };

    retrigger.onTick = [this] { engine.play(); };

    setWantsKeyboardFocus (true);
    startTimerHz (30);
    setSize (1380, 860);
    applyVisibility();
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

    topBar.setViewLit (0, showSample);
    topBar.setViewLit (1, showTrans);
    topBar.setViewLit (2, showFx);
    topBar.setViewLit (3, showMutate);
    topBar.setViewLit (4, showVars);
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

    // Right region: VARIATIONS (hiding it gives the left full width).
    auto body = area;
    auto left = body;
    if (showVars)
    {
        left = body.removeFromLeft (juce::roundToInt (body.getWidth() * dim::leftFrac));
        variations.setBounds (body);
    }

    auto L = left.reduced (12, 12);

    // MUTATE strip pinned to the bottom.
    if (showMutate) { mutate.setBounds (L.removeFromBottom (88)); L.removeFromBottom (10); }

    // Top-to-bottom: SAMPLE, TRANSFORMERS, PREP (always), FX (rack + detail).
    // FX is the primary flexible filler; if FX is hidden, SAMPLE expands to fill instead.
    const int transH = 130, prepH = 92, sampleDefault = 150;
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
    topBar.repaint();
    source.repaint();
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
