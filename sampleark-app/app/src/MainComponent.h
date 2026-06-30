#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Panels.h"
#include "AudioEngine.h"
#include "Variations.h"
#include <vector>

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
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void openChooser();
    void showAudioSettings();                 // standard JUCE audio-interface picker dialog
    void exportPrepped();
    void setPlayEvery (int comboId);          // 1 off, 2 quarter, 3 half, 4 bar
    double intervalMsFor (int comboId) const; // retrigger interval (0 = off)
    void startPlayback();                     // PLAY / P: (re)start, honouring the armed play-every mode
    void stopAll();                           // STOP / S: halt transport (play-every stays armed)

    int playEveryId = 1;                      // persistent "Play every" setting (survives STOP)

    // tempo-synced auto-retrigger ("Play every")
    struct RetriggerTimer : juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { if (onTick) onTick(); }
    };
    RetriggerTimer retrigger;
    int lastRenderVer = 0;
    bool focusGrabbed = false;

    AudioEngine engine;

    void toggleZone (int zone);     // 0 sample, 1 trans, 2 fx, 3 mutate, 4 vars, 5 inputs
    void applyVisibility();

    TopBar topBar;
    SourcePanel source;
    TransformerPanel transformers;
    PrepPanel prep;
    RackPanel rack;
    DetailPanel detail;
    MutateStrip mutate;
    VariationsPanel variations;
    InputsPanel inputs;

    bool showSample = true, showTrans = false, showFx = true, showMutate = true, showVars = true;
    bool showInputs = false;

    // M4 variations
    void promptMutate();                     // MUTATE: ask add/replace when candidates already exist
    void runMutate (bool replace);           // generate a batch (replace clears, else append)
    void captureCurrent();                   // + ADD THIS: snapshot live settings as a new variation
    int  nextVariationNumber() const;        // lowest free number (01 on an empty list)
    void auditionVariation (int i);          // play a candidate's audio (no recipe change)
    void recallVariation (int i);            // load a candidate's recipe into the rack + output, audition it
    void toggleFavourite (int i);            // mark/unmark for write (capped)
    void writeSelected();
    std::vector<Variation> variationList;    // row 0 = Baseline (when present), then candidates
    int lastAuditioned = -1;
    static constexpr int kMaxFavourites = 8; // hard cap on favourites (later configurable)

    // PLAY ALL: audition every (non-muted) variation in turn with a short gap between them.
    void togglePlaylist();
    void playlistAdvance();
    bool playlistActive = false;
    int  playlistIdx = -1;
    double playlistWaitMs = 0.0;             // 0 = current still playing; >0 = gap deadline

    // Blueprints: 8 favourite mutation setups — depth + affects scope + a rack snapshot.
    struct Blueprint
    {
        bool filled = false;
        float depth = 0.45f;
        ScopeMask scope {};
        PrepParams prep;
        FxRack rack;
        std::array<Transformer, kNumTransformers> trans;
    };
    std::array<Blueprint, 8> blueprints;
    void saveBlueprint (int i);
    void recallBlueprint (int i);
    void refreshMutateInfo();                // push zone/% to the variations panel

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
}
