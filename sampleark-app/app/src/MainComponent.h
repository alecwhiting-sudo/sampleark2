#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Panels.h"
#include "AudioEngine.h"

namespace sa
{
// Root content component. Owns the audio engine + the screen zones, lays them out per
// Arch04, and (M1) wires the toolbar + SAMPLE panel to real load/playback. Accepts
// dropped audio files and a command-line file path.
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void loadFile (const juce::File& file);   // also used for command-line / "open with"

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void openChooser();

    AudioEngine engine;

    TopBar topBar;
    SourcePanel source;
    PrepPanel prep;
    RackPanel rack;
    DetailPanel detail;
    MutateStrip mutate;
    VariationsPanel variations;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
}
