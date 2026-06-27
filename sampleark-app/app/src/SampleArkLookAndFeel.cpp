#include "SampleArkLookAndFeel.h"
#include "Theme.h"

namespace sa
{
SampleArkLookAndFeel::SampleArkLookAndFeel()
{
    using namespace sa::theme::colour;

    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::DocumentWindow::backgroundColourId, bg);

    setColour (juce::Label::textColourId, ink);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId, buttonNeutral);
    setColour (juce::TextButton::textColourOffId, ink);

    setColour (juce::ScrollBar::backgroundColourId, panelAlt);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff38352f));
}

juce::Font SampleArkLookAndFeel::getTextButtonFont (juce::TextButton&, int /*buttonHeight*/)
{
    return sa::theme::monoFont (11.0f, true);
}
}
