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
    void setViewLit (int zone, bool lit);   // colour a view-toggle (0 sample,1 trans,2 fx,3 mutate,4 vars)

    std::function<void()> onPlay, onStop, onLoad, onLoop;
    std::function<void(int)> onEvery;   // combo id: 1 off, 2 quarter, 3 half, 4 bar
    std::function<void(int)> onView;    // view-toggle clicked (zone index 0..4)

private:
    AudioEngine* engine = nullptr;
    FlatButton playB { "> PLAY" }, stopB { "STOP" }, loopB { "LOOP" }, loadB { "(+) LOAD SAMPLE" };
    FlatButton vSample { "SMPL" }, vTrans { "TRANS" }, vFx { "FX" }, vMut { "MUT" }, vVars { "VARS" };
    juce::ComboBox everyBox;
};

// SAMPLE panel: SOURCE / OUTPUT / BOTH views. SOURCE = original (granular + trim handles);
// OUTPUT = the rendered region+tail ("the new sample") with the true playhead. Each lane
// auto-fits its own content to its width (independent zoom) — no manual zooming.
class SourcePanel : public juce::Component
{
public:
    void setEngine (AudioEngine* e) { engine = e; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void drawSource (juce::Graphics&, juce::Rectangle<float> lane);
    void drawOutput (juce::Graphics&, juce::Rectangle<float> lane);
    juce::Rectangle<float> sourceWaveArea() const;   // trim mouse mapping (empty if source not shown)
    juce::Rectangle<int> toggleBounds() const;

    AudioEngine* engine = nullptr;
    int dragHandle = 0;  // 0 none, 1 start, 2 end
    int viewMode = 2;    // 0 source, 1 output, 2 both
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
    Knob* startK = nullptr; Knob* endK = nullptr; Knob* fiK = nullptr; Knob* foK = nullptr;
    Knob* ofiK = nullptr; Knob* ofoK = nullptr; Knob* gainK = nullptr;
    FlatButton* normT = nullptr;
};

// FX RACK (M3): 8 slots — click to select, click the LED to bypass, drag the handle to reorder.
class RackPanel : public juce::Component
{
public:
    void setEngine (AudioEngine* e) { engine = e; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    int rowAt (juce::Point<int> p) const;
    AudioEngine* engine = nullptr;
    int dragRow = -1;
};

// Selected-effect editor (M3): live knobs + per-effect graph for the selected slot.
class DetailPanel : public juce::Component
{
public:
    void setEngine (AudioEngine* e) { engine = e; buildEditor(); }
    void refresh();                 // rebuild on selection change, else sync values
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    void buildEditor();
    void applyEditorLooks();
    void handleGraphDrag (const juce::MouseEvent&);
    juce::Rectangle<float> graphBounds() const;

    AudioEngine* engine = nullptr;
    int builtSlot = -1;
    juce::OwnedArray<Knob> knobs;
    std::vector<int> knobParamIndex;
    juce::OwnedArray<FlatButton> segButtons;   // all seg buttons, concatenated by group
    std::vector<int> segParams;                // param index per seg group
    std::vector<int> segCounts;                // option count per group (parallel)
};

class MutateStrip    : public juce::Component { public: void paint (juce::Graphics&) override; };
class VariationsPanel: public juce::Component { public: void paint (juce::Graphics&) override; };

// Transformer dock (M3a): up to 8 modulation lanes. The active lane shows an editable shape
// graph (draw to override the preset) + target/shape/depth/basis controls; the lane buttons
// select/enable each transformer.
class TransformerPanel : public juce::Component
{
public:
    TransformerPanel();
    void setEngine (AudioEngine* e) { engine = e; rebuildTargets(); refresh(); }
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> graphBounds() const;
    void rebuildTargets();
    void drawCurveTo (const juce::MouseEvent&);

    struct TargetRef { int kind; int slot; int param; };   // kind: 0 effect, 1 pre-amp, 2 post-amp

    AudioEngine* engine = nullptr;
    int active = 0;

    juce::OwnedArray<FlatButton> laneBtns;
    FlatButton onBtn { "ON" };
    FlatButton basisOne { "1-Shot" }, basisCyc { "Cyclic" };
    juce::ComboBox targetBox, shapeBox, rateBox;
    Knob depthKnob { "Depth" }, freqKnob { "Freq" }, phaseKnob { "Phase" };
    std::vector<TargetRef> targetRefs;
};
}
