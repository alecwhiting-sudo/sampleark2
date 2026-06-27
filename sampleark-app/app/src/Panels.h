#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// M0 static panels. Each is a Component that paints one zone of the main screen per the
// Arch04 spec. They are intentionally static (no data wiring, no interaction) — M0 proves
// the layout, density, and style. M1+ replaces the painted placeholders with live data
// (real waveform, real rack state, real variations) and interactive child components.
namespace sa
{
class TopBar         : public juce::Component { public: void paint (juce::Graphics&) override; };
class SourcePanel    : public juce::Component { public: void paint (juce::Graphics&) override; };
class PrepPanel      : public juce::Component { public: void paint (juce::Graphics&) override; };
class RackPanel      : public juce::Component { public: void paint (juce::Graphics&) override; };
class DetailPanel    : public juce::Component { public: void paint (juce::Graphics&) override; };
class MutateStrip    : public juce::Component { public: void paint (juce::Graphics&) override; };
class VariationsPanel: public juce::Component { public: void paint (juce::Graphics&) override; };
}
