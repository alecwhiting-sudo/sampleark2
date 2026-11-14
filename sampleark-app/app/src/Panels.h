#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatButton.h"
#include "Knob.h"

// Main-screen zones. Toolbar + SAMPLE + PREP are live (M1/M2); rack (M3) and
// mutate/variations (M4) remain painted placeholders until their milestone.
namespace sa
{
class AudioEngine; // fwd
struct Variation;  // fwd (M4)

// The TEMPO chip's BPM readout (M6): drag up/down to change (Shift = fine, 0.1 BPM), mouse
// wheel steps ±1 (Shift ±0.1), double-click to type a value.
class TempoField : public juce::Label
{
public:
    TempoField();
    void setBpm (double bpm);           // <= 0 shows "--"
    void setDragEnabled (bool b);       // false: type-only (double-click), no drag / wheel
    std::function<void(double)> onBpm;      // requested value (the engine clamps + rounds)

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void textWasEdited() override;
    double bpm = 120.0, dragStartBpm = 120.0;
    float wheelAcc = 0.0f;              // trackpad deltas accumulated towards the next step
    bool dragEnabled = true;
};

// PITCH tab: the loop's ORIGINAL tempo — a fact about the recording, set by detection, not a
// control you play. Deliberately not a knob: turning "the loop's tempo" down makes the loop play
// FASTER (the session tempo is divided by it), which reads as backwards. Type it (double-click)
// or fix an octave mistake with /2 and x2; the TEMPO box in the toolbar is the one you move.
class OriginalTempo : public juce::Component
{
public:
    OriginalTempo();
    void setBpm (double bpm) { field.setBpm (bpm); }
    std::function<void(double)> onBpm;          // typed / halved / doubled value (0 = unknown)
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    TempoField field;
    FlatButton halfB { "/2" }, dblB { "x2" };
};

// PITCH tab: the TARGET tempo — where the loop is moved to. The same value as the toolbar TEMPO
// (the session tempo), shown next to DETECTED so the retime reads left to right: 120 -> 140.
// Type it (double-click), drag / scroll it, or step with - / +. Higher = faster.
class TargetTempo : public juce::Component
{
public:
    TargetTempo();
    void setBpm (double bpm) { current = bpm; field.setBpm (bpm); }
    void setLive (bool l)    { live = l; repaint(); }   // false: no detected tempo, so nothing to move
    std::function<void(double)> onBpm;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    TempoField field;
    FlatButton downB { "-" }, upB { "+" };
    double current = 120.0;
    bool live = true;
};

class TopBar : public juce::Component
{
public:
    TopBar();
    void setEngine (AudioEngine* e) { engine = e; refresh(); }
    void refresh();                 // reflect loop state on the LOOP button
    void resized() override;
    void paint (juce::Graphics&) override;
    void setEveryOff() { everyBox.setSelectedId (1, juce::dontSendNotification); }
    void setViewLit (int zone, bool lit);   // colour a view-toggle (0 sample,1 trans,2 fx,3 mutate,4 vars,5 inputs)
    void setHistory (bool canUndo, bool canRedo);   // dim UNDO/REDO when there is nothing to step to
    void flashMessage (const juce::String&);        // transient line in the filename chip ("undo  Gain")
    void tickFlash();                               // called from the main timer; clears an expired flash

    std::function<void()> onPlay, onStop, onLoad, onLoop, onAudio, onUndo, onRedo;
    std::function<void(int)> onEvery;   // combo id: 1 off, 2 quarter, 3 half, 4 bar
    std::function<void(int)> onView;    // view-toggle clicked (zone index 0..5)
    std::function<void(double)> onTempo;// session tempo requested (steppers, drag, wheel, typed)
    std::function<void()> onDetect;     // DETECT: analyse the region's tempo

private:
    AudioEngine* engine = nullptr;
    FlatButton playB { "> PLAY" }, stopB { "STOP" }, loopB { "LOOP" }, loadB { "(+) LOAD SAMPLE" }, audioB { "AUDIO" };
    FlatButton undoB { "UNDO" }, redoB { "REDO" };
    juce::String flash;                 // transient message shown instead of the file meta line
    juce::uint32 flashUntil = 0;
    FlatButton vInputs { "INPUTS" }, vSample { "SMPL" }, vTrans { "TRANS" }, vFx { "FX" }, vMut { "MUT" }, vVars { "VARS" };
    juce::ComboBox everyBox;
    FlatButton tempoDown { "-" }, tempoUp { "+" }, detectB { "DETECT" };
    TempoField tempoField;
    juce::Rectangle<int> tempoChip() const;
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

    // Transformer overlay: when >= 0, that transformer's curve is drawn over the OUTPUT
    // lane and is editable there (used by the Overlay view mode). -1 = off.
    void setOverlayTransformer (int idx) { overlayTrans = idx; repaint(); }
    void ensureOutputVisible () { if (viewMode == 0) { viewMode = 2; repaint(); } }

private:
    void drawSource (juce::Graphics&, juce::Rectangle<float> lane);
    void drawOutput (juce::Graphics&, juce::Rectangle<float> lane);
    void drawTransformerOverlay (juce::Graphics&, juce::Rectangle<float> waveArea);
    juce::Rectangle<float> sourceWaveArea() const;   // trim mouse mapping (empty if source not shown)
    juce::Rectangle<float> outputWaveArea() const;   // overlay-edit mapping (empty if output not shown)
    bool editOverlayAt (const juce::MouseEvent&);    // edit the active curve; true if handled
    juce::Rectangle<int> toggleBounds() const;
    juce::Rectangle<int> stereoToggleBounds() const; // STEREO / COMBINED channel display toggle

    AudioEngine* engine = nullptr;
    int dragHandle = 0;  // 0 none, 1 start, 2 end
    int viewMode = 2;    // 0 source, 1 output, 2 both
    int overlayTrans = -1;
    bool stereoView = true;   // SOURCE shows two channels; false = combined (summed)
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
    // PITCH (M6)
    Knob* transK = nullptr; Knob* tuneK = nullptr; Knob* formK = nullptr;
    std::unique_ptr<OriginalTempo> origT;
    std::unique_ptr<TargetTempo> targetT;
    FlatButton* warpT = nullptr; FlatButton* resampT = nullptr; FlatButton* followT = nullptr;
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    int rowAt (juce::Point<int> p) const;
    int maxScroll() const;                         // 0 when all rows fit
    AudioEngine* engine = nullptr;
    int dragRow = -1;
    int scrollY = 0;                               // vertical scroll offset (px)
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void buildEditor();
    void applyEditorLooks();
    void handleGraphDrag (const juce::MouseEvent&);
    juce::Rectangle<float> graphBounds() const;
    int graphHeight() const;        // graph height after squeeze floor
    int contentHeight() const;      // natural height of header + graph + controls
    int maxScroll() const;

    AudioEngine* engine = nullptr;
    int builtSlot = -1;
    int builtType = -1;             // effect type the editor was built for; a reorder can change it
    int scrollY = 0;
    juce::OwnedArray<Knob> knobs;
    std::vector<int> knobParamIndex;
    juce::OwnedArray<FlatButton> segButtons;   // all seg buttons, concatenated by group
    std::vector<int> segParams;                // param index per seg group
    std::vector<int> segCounts;                // option count per group (parallel)

    // TAIL row (Delay/Reverb only): how much this effect may extend the sample. Backed by the
    // slot's tail mode + amount rather than an FxParam, so mutation never touches it.
    juce::OwnedArray<FlatButton> tailButtons;  // OFF / FULL / TIME / xLOOP
    std::unique_ptr<Knob> tailKnob;            // the TIME / xLOOP amount

    // Dynamics meter ballistics (peak-hold + decay) for the displayed comp/limiter slot.
    int    meterSlot   = -1;                   // resets the holds when the shown slot changes
    float  outPeakDb   = -120.0f, grPeakDb = 0.0f;
    double outStampMs  = 0.0, grStampMs = 0.0; // when each held peak was last set
    double clipStampMs = -1.0e9;               // last time OUT came within 2 dB of 0 dBFS
};

// MUTATE strip (M4): DEPTH severity slider (one continuous scale, named zones) + AFFECTS scope
// chips. Drives the variation generator; exposes the chosen level + scope flags.
class MutateStrip : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    float level() const { return depth; }            // 0..1 severity
    bool  scope (int i) const { return i >= 0 && i < 10 && scopeFlags[i]; }
    std::function<void()> onChanged;

private:
    float depth = 0.45f;
    bool  scopeFlags[10] = { true, false, false, false, false, false, false, false, false, false };
    juce::Rectangle<float> trackRect;        // cached in paint for hit-testing
    juce::Rectangle<int>   chipRects[10];
};

// VARIATIONS browser (M4): rendered candidate rows (mini-waveform, name, select/mute, audition)
// + MUTATE / WRITE / KEEP-PLAYING footer. Reads a Variation list owned by MainComponent.
class VariationsPanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setData (const std::vector<Variation>* v) { vars = v; repaint(); }
    void setStatus (juce::String s) { status = std::move (s); repaint(); }
    void setActive (int row) { activeRow = row; repaint(); }   // the recipe currently loaded into the rack
    void setMutateInfo (juce::String s) { mutateInfo = std::move (s); repaint(); }   // "Gentle 18%" echo on the button

    void setPlaylistActive (bool a) { playlistActive = a; repaint(); }

    std::function<void()>    onMutate, onWrite, onPlayAll, onAddThis;
    std::function<void(int)> onRecall, onToggleSelect, onToggleMute;

private:
    int rowAt (juce::Point<int>) const;
    int listTop() const { return 48; }
    int footerH() const { return 100; }
    int maxScroll() const;
    juce::Rectangle<int> mutateBtn() const;
    juce::Rectangle<int> addBtn() const;
    juce::Rectangle<int> writeBtn() const;
    juce::Rectangle<int> playAllBtn() const;

    const std::vector<Variation>* vars = nullptr;
    juce::String status, mutateInfo;
    int scrollY = 0;
    int activeRow = -1;     // row whose recipe is loaded into the live rack/output (-1 none)
    bool playlistActive = false;
};

// INPUTS browser (M3b): pick a folder and browse/audition source samples in-app. Lives in the
// right region alongside VARIATIONS; clicking a file loads it as the source, Up/Down step through.
class InputsPanel : public juce::Component
{
public:
    InputsPanel();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverRow = -1; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setFolder (const juce::File&);
    void setLoaded (const juce::File& f) { loaded = f; repaint(); }
    void step (int dir);    // move selection among files and load it (arrow-key audition)

    std::function<void(const juce::File&)> onLoadFile;

private:
    void refreshListing();
    int  listTop() const { return 56; }     // header + path line
    int  rowAt (juce::Point<int>) const;
    int  rowCount() const;                   // [..] + dirs + files
    int  maxScroll() const;
    void activateRow (int row);
    juce::Rectangle<int> folderBtnBounds() const;

    juce::File folder, loaded;
    juce::Array<juce::File> dirs, files;
    bool hasParent = false;
    int scrollY = 0, hoverRow = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};

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

    int  activeIndex() const { return active; }
    bool isOverlay()   const { return overlay; }
    std::function<void(bool)> onOverlayToggle;   // user flipped Separate <-> Overlay
    std::function<void(int)>  onActiveChanged;   // active lane changed (keep overlay in sync)

private:
    juce::Rectangle<float> graphBounds() const;  // editable curve area (empty in overlay mode)
    void rebuildTargets();
    void drawCurveTo (const juce::MouseEvent&);

    struct TargetRef { int kind; int slot; int param; };   // kind: 0 effect, 1 pre-amp, 2 post-amp

    AudioEngine* engine = nullptr;
    int active = 0;
    bool overlay = false;                        // graph drawn over the SAMPLE waveform instead

    juce::OwnedArray<FlatButton> laneBtns;
    FlatButton onBtn { "ON" };
    FlatButton basisOne { "1-Shot" }, basisCyc { "Cyclic" };
    FlatButton viewBtn { "Overlay" };            // toggles Separate <-> Overlay (temporary eval control)
    juce::ComboBox targetBox, shapeBox, rateBox;
    Knob depthKnob { "Depth" }, freqKnob { "Freq" }, phaseKnob { "Phase" };
    std::vector<TargetRef> targetRefs;
};
}
