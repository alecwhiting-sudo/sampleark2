#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <cmath>

namespace sa
{
// Interactive rotary knob in the SampleArk style: vertical-drag to turn (≈170px = full
// 0->1, per Arch04), double-click to reset to default. Sits on an optional core-tinted
// plate with a label beneath. Value is normalised 0..1; callers map to real units.
class Knob : public juce::Component
{
public:
    explicit Knob (juce::String labelText) : label (std::move (labelText)) {}

    void setValue (float v, bool notify = false)
    {
        value = juce::jlimit (0.0f, 1.0f, v);
        if (notify && onValueChange) onValueChange (value);
        repaint();
    }
    float getValue() const noexcept { return value; }
    void setDefault (float d)       { defaultValue = d; }
    void setCore (bool c)           { core = c; repaint(); }
    void setLabel (juce::String l)  { label = std::move (l); repaint(); }
    void setInert (bool i)          { inert = i; repaint(); }   // not-yet-wired: dimmed, no input

    std::function<void(float)> onValueChange;
    std::function<juce::String(float)> valueText;   // 0..1 -> readout (e.g. "1.8 Hz"); shown on hover/drag

    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (inert) return;
        dragging = true;
        dragStartY = e.position.y;
        dragStartValue = value;
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (inert) return;
        const float dv = (dragStartY - e.position.y) / 170.0f;
        setValue (dragStartValue + dv, true);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (inert) return;
        dragging = false;
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (inert) return;
        setValue (defaultValue, true);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace sa::theme;
        auto r = getLocalBounds().toFloat();

        // plate
        g.setColour (core ? colour::accentTint : colour::panelAlt);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (core ? colour::accent.withAlpha (0.45f) : colour::borderSubtle);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        auto labelArea = getLocalBounds().removeFromBottom (16);
        const float size = (float) juce::jmin (getWidth() - 12, getHeight() - 22);
        juce::Rectangle<float> dial ((float) getWidth() * 0.5f - size * 0.5f,
                                     (float) (getHeight() - 16) * 0.5f - size * 0.5f + 2.0f,
                                     size, size);

        g.setColour (juce::Colour (0xff1a1916));
        g.fillEllipse (dial);
        g.setColour (juce::Colour (0xff34322e));
        g.drawEllipse (dial.reduced (0.5f), 1.0f);

        const float a0 = juce::degreesToRadians (-135.0f);
        const float a1 = juce::degreesToRadians (135.0f);
        const float av = a0 + value * (a1 - a0);
        const float rad = dial.getWidth() * 0.5f;

        juce::Path arc;
        arc.addCentredArc (dial.getCentreX(), dial.getCentreY(), rad - 2.0f, rad - 2.0f,
                           0.0f, a0, av, true);
        g.setColour (core ? colour::accent : juce::Colour (0xff8a847c));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const float ix = dial.getCentreX() + std::sin (av) * (rad - 5.0f);
        const float iy = dial.getCentreY() - std::cos (av) * (rad - 5.0f);
        g.setColour (colour::ink);
        g.drawLine (dial.getCentreX(), dial.getCentreY(), ix, iy, 2.0f);

        g.setColour (core ? colour::accent : colour::faint);
        g.setFont (monoFont (8.0f));
        g.drawFittedText (label, labelArea, juce::Justification::centred, 2);   // wraps long names

        if (inert)   // veil to read as disabled / not-yet-wired
        {
            g.setColour (colour::panel.withAlpha (0.58f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        else if (valueText && (hovering || dragging))   // value readout pill over the dial top
        {
            const juce::String vt = valueText (value);
            g.setFont (monoFont (8.5f, true));
            const float tw = juce::jmax (28.0f, g.getCurrentFont().getStringWidthFloat (vt) + 8.0f);
            juce::Rectangle<float> pill ((float) getWidth() * 0.5f - tw * 0.5f, dial.getY() - 5.0f, tw, 13.0f);
            pill = pill.constrainedWithin (r);
            g.setColour (juce::Colour (0xff000000).withAlpha (0.82f));
            g.fillRoundedRectangle (pill, 3.0f);
            g.setColour (colour::accent);
            g.drawText (vt, pill, juce::Justification::centred);
        }
    }

private:
    juce::String label;
    float value = 0.5f, defaultValue = 0.5f;
    bool core = true;
    bool inert = false;
    bool hovering = false, dragging = false;
    float dragStartY = 0.0f, dragStartValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
}
