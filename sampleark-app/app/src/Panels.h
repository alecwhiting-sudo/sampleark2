#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatButton.h"

// Main-screen zones. Panels still painted in the Arch04 style; M1 makes the toolbar
// and SAMPLE panel live (real transport + waveform), the rest remain static until their
// milestone (rack M3, mutate/variations M4, prep M2).
namespace sa
{
class AudioEngine; // fwd

class TopBar : public juce::Component
{
public:
    TopBar();
    void setEngine (AudioEngine* e) { engine = e; }
    void resized() override;
    void paint (juce::Graphics&) override;

    std::function<void()> onPlay, onStop, onLoad;

private:
    AudioEngine* engine = nullptr;
    FlatButton playB { "> PLAY" }, stopB { "STOP" }, loadB { "(+) LOAD SAMPLE" };
};

class SourcePanel : public juce::Component
{
public:
    void setEngine (AudioEngine* e) { engine = e; }
    void paint (juce::Graphics&) override;
private:
    AudioEngine* engine = nullptr;
};

class PrepPanel      : public juce::Component { public: void paint (juce::Graphics&) override; };
class RackPanel      : public juce::Component { public: void paint (juce::Graphics&) override; };
class DetailPanel    : public juce::Component { public: void paint (juce::Graphics&) override; };
class MutateStrip    : public juce::Component { public: void paint (juce::Graphics&) override; };
class VariationsPanel: public juce::Component { public: void paint (juce::Graphics&) override; };
}
