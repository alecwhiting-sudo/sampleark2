#include "MainComponent.h"
#include "Theme.h"

namespace sa
{
MainComponent::MainComponent()
{
    topBar.setEngine (&engine);
    source.setEngine (&engine);

    addAndMakeVisible (topBar);
    addAndMakeVisible (source);
    addAndMakeVisible (prep);
    addAndMakeVisible (rack);
    addAndMakeVisible (detail);
    addAndMakeVisible (mutate);
    addAndMakeVisible (variations);

    topBar.onPlay = [this] { engine.togglePlay(); topBar.repaint(); };
    topBar.onStop = [this] { engine.stop(); source.repaint(); };
    topBar.onLoad = [this] { openChooser(); };
    engine.onChange = [this] { topBar.repaint(); source.repaint(); };

    startTimerHz (30);
    setSize (1380, 860);
}

MainComponent::~MainComponent()
{
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

    auto body = area;
    auto left = body.removeFromLeft (juce::roundToInt (body.getWidth() * dim::leftFrac));
    variations.setBounds (body); // right region

    auto L = left.reduced (12, 12);
    source.setBounds (L.removeFromTop (150));
    L.removeFromTop (10);
    prep.setBounds (L.removeFromTop (92));
    L.removeFromTop (10);
    mutate.setBounds (L.removeFromBottom (88)); // two rows: depth + affects
    L.removeFromBottom (10);

    auto middle = L;
    rack.setBounds (middle.removeFromLeft (juce::roundToInt (middle.getWidth() * 0.43f)));
    middle.removeFromLeft (10);
    detail.setBounds (middle);
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
    // Advance the playhead while playing; also catch the async thumbnail finishing.
    if (engine.isPlaying())
        source.repaint();
    else if (engine.hasFile() && ! engine.thumbnail().isFullyLoaded())
        source.repaint();
}
}
