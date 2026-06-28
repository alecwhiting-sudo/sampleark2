#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatButton.h"
#include "Knob.h"

// Main-screen zones. Toolbar + SAMPLE + PREP are live (M1/M2); rack (M3) and
// mutate/variations (M4) remain painted placeholders until their milestone.
namespace sa
{
class AudioEngine; // fwd

class TopBar : public juce::Component
{
public:
    TopBar();
    void setEngine (AudioEngine* e) { engine = e; refresh(); }
    void refresh();                 // reflect loop state on the LOOP button
    void resized() override;
    void paint (juce::Graphics&) override;
    void setEveryOff() { everyBox.setSelectedId (1, juce::dontSendNotification); }

    std::function<void()> onPlay, onStop, onLoad, onLoop;
    std::function<void(int)> onEvery;   // combo id: 1 off, 2 quarter, 3 half, 4 bar

private:
    AudioEngine* engine = nullptr;
    FlatButton playB { "> PLAY" }, stopB { "STOP" }, loopB { "LOOP" }, loadB { "(+) LOAD SAMPLE" };
    juce::ComboBox everyBox;
};

class SourcePanel : public juce::Component
{
public:
    void setEngine (AudioEngine* e) { engine = e; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> waveArea() const;  // waveform drawing rect (for x<->frac mapping)
    AudioEngine* engine = nullptr;
    int dragHandle = 0; // 0 none, 1 start, 2 end
};

// PREP panel (M2): TRIM / SHAPE / PITCH modes with live knobs wired to the engine.
class PrepPanel : public juce::Component
{
public:
    PrepPanel();
    void setEngine (AudioEngine* e) { engine = e; buildMode(); }
    void refresh();                 // sync knob values/looks from engine
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void buildMode();               // (re)create controls for the active mode
    void applyLooks();

    AudioEngine* engine = nullptr;
    int mode = 0;                   // 0 trim, 1 shape, 2 pitch

    juce::OwnedArray<FlatButton> tabs;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<FlatButton> toggles;

    // pointers into the OwnedArrays for the wired controls (null when not in mode)
    Knob* startK = nullptr; Knob* endK = nullptr; Knob* fiK = nullptr; Knob* foK = nullptr; Knob* gainK = nullptr;
    FlatButton* normT = nullptr;
};

class RackPanel      : public juce::Component { public: void paint (juce::Graphics&) override; };
class DetailPanel    : public juce::Component { public: void paint (juce::Graphics&) override; };
class MutateStrip    : public juce::Component { public: void paint (juce::Graphics&) override; };
class VariationsPanel: public juce::Component { public: void paint (juce::Graphics&) override; };
}
