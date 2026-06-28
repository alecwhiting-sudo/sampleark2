#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace sa
{
// A clickable button painted in the SampleArk hardware style (rounded plate + mono
// label). Replaces the painted pseudo-buttons where real interaction is needed.
class FlatButton : public juce::Button
{
public:
    explicit FlatButton (const juce::String& text) : juce::Button (text) { setButtonText (text); }

    void setColours (juce::Colour bg, juce::Colour border, juce::Colour text)
    {
        bgCol = bg; borderCol = border; textCol = text; repaint();
    }
    void setFontSize (float s) { fontSize = s; }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        auto fill = down ? bgCol.darker (0.2f) : (over ? bgCol.brighter (0.06f) : bgCol);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);
        if (! borderCol.isTransparent())
        {
            g.setColour (borderCol);
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        }
        g.setColour (isEnabled() ? textCol : textCol.withAlpha (0.4f));
        g.setFont (sa::theme::monoFont (fontSize, true));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }

private:
    juce::Colour bgCol     { sa::theme::colour::buttonNeutral };
    juce::Colour borderCol { sa::theme::colour::border };
    juce::Colour textCol   { sa::theme::colour::ink };
    float fontSize = 11.0f;
};
}
