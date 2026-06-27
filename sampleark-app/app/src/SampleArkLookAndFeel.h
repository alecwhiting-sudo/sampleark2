#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// App-wide LookAndFeel. For M0 it mainly establishes default colours so any standard
// JUCE widgets we add later inherit the dark/hardware palette. Most of the dense custom
// UI is drawn directly in the panels.
namespace sa
{
class SampleArkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SampleArkLookAndFeel();
    ~SampleArkLookAndFeel() override = default;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
}
