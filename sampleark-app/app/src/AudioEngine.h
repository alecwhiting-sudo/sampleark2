#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "FxRack.h"
#include "Transformer.h"
#include <functional>
#include <atomic>
#include <array>
#include <deque>

namespace sa
{
// One-shot prep parameters (M2). Normalised where noted; mapped to real units by the UI.
struct PrepParams
{
    double startFrac = 0.0;   // region start as fraction of source
    double endFrac   = 1.0;   // region end
    double fadeInMs  = 2.0;   // SOURCE fades (pre-effect, at the trim edges) — click protection
    double fadeOutMs = 4.0;
    double outFadeInMs  = 0.0;// OUTPUT fades (post-effect, on the rendered result incl. tail)
    double outFadeOutMs = 0.0;// fade-out anchors to the dynamic output end
    double gainDb    = 0.0;
    bool   normalize = false;
};

// M1 audible spine + M2 one-shot prep. Decodes a sample fully into RAM, renders the
// prepped one-shot (trim + fades + gain + normalize) into a play buffer, triggers it with
// zero latency, draws it, and exports it to WAV.
class AudioEngine : private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    bool loadFile (const juce::File& file);
    void play();
    void stop();
    void togglePlay();
    bool isPlaying() const;

    void setLoop (bool shouldLoop);
    bool isLoopOn() const { return loopOn.load(); }

    // The shared device manager, for the in-app audio-interface selector.
    juce::AudioDeviceManager& audioDeviceManager() { return deviceManager; }
    double tempo() const  { return tempoBpm; }   // fixed 120 until M6 wires tempo

    // --- prep (M2) ---
    void setTrim (double startFrac, double endFrac);
    void setFadeInMs (double ms);
    void setFadeOutMs (double ms);
    void setOutFadeInMs (double ms);
    void setOutFadeOutMs (double ms);
    void setGainDb (double db);
    void setNormalize (bool on);
    const PrepParams& prep() const            { return prepParams; }

    // --- rack (M3) ---
    const FxRack& rack() const { return fxRack; }
    int  selectedSlot() const  { return selSlot; }
    void selectSlot (int s);
    void rackToggleBypass (int slot);
    void rackMove (int from, int to);
    void rackSetParam (int slot, int paramIndex, float value);
    void rackSetTailMode (int slot, TailMode);
    void rackSetTailAmount (int slot, float amount01);

    // Live transformer feedback for the FX-graph: the selected slot's params with modulation
    // applied at the current playback position (returns the base params when not playing), plus
    // a test for whether any ON transformer targets the slot (so the UI can animate only then).
    std::vector<float> liveParams (int slot) const;
    bool isSlotModulated (int slot) const;

    // Dynamics metering captured during the main render (Compression/Limiter editors). Output
    // level is post-effect (post-makeup) dBFS; gain reduction is dB (>= 0). "At" variants read the
    // value at a playhead position for a live meter; "Peak"/"Max" hold the loudest over the render.
    bool  hasMeter (int slot) const;
    float meterOutDbAt (int slot, int posSamples) const;
    float meterGrDbAt  (int slot, int posSamples) const;
    float meterPeakOutDb (int slot) const;
    float meterMaxGrDb   (int slot) const;

    // --- transformers (M3a) ---
    const std::array<Transformer, kNumTransformers>& transformers() const { return transformerArray; }
    void setTransformer (int index, const Transformer& t);

    // --- edit history (undo/redo) ---
    // Covers the editor state the user builds by hand: prep + rack + transformers — the same
    // triple applyRecipe swaps and a variation snapshots. Selection, transport and the variations
    // workspace are deliberately not undoable: they are where you are, not what you made.
    // A continuous gesture (knob drag, handle drag, curve draw) collapses into ONE step.
    bool canUndo() const                      { return ! undoStack.empty(); }
    bool canRedo() const                      { return ! redoStack.empty(); }
    juce::String undoName() const             { return undoStack.empty() ? juce::String() : undoStack.back().desc; }
    juce::String redoName() const             { return redoStack.empty() ? juce::String() : redoStack.back().desc; }
    bool undo();
    bool redo();

    // --- recipe recall (M4 variations) ---
    // Drop a whole (prep + rack + transformers) snapshot into the live state in one shot, then
    // re-render. Used when the user selects a variation/Baseline row to load it into the editors.
    void applyRecipe (const PrepParams&, const FxRack&,
                      const std::array<Transformer, kNumTransformers>&);

    // --- variations (M4) ---
    // Render an arbitrary (prep, rack, transformer) state to its own stereo buffer (region + tail).
    // Returns the valid length; `out` is resized. Lets the variation system render many candidates.
    int  renderState (const PrepParams&, const FxRack&,
                      const std::array<Transformer, kNumTransformers>&, double tempo,
                      juce::AudioBuffer<float>& out) const;
    void auditionBuffer (const juce::AudioBuffer<float>& buf, int len);   // play a candidate's audio
    bool writeWav (const juce::File&, const juce::AudioBuffer<float>&, int len, int bitDepth) const;

    bool exportPreppedTo (const juce::File& file, int bitDepth);

    // --- tail (output-length policy) ---
    // The published sample stops at the output length; the ring-out past it is still rendered and
    // kept in the retained-tail buffer (pristine — no output fade baked in) for the features that
    // will use it (ghosting it behind the cut, wrapping it into a loop, tail-only layers).
    double discardedTailSeconds() const;                       // audio computed but not published
    std::vector<float> discardedTailPeaks (int numColumns) const;   // for drawing it ghosted

    bool hasFile() const                      { return sampleBuffer.getNumSamples() > 0; }
    juce::AudioThumbnail& thumbnail()         { return thumb; }

    // Peak-per-column of the rendered output (prepped region + effect tail) for drawing the
    // OUTPUT waveform. Read-only on the message thread (safe vs the audio thread's reads).
    std::vector<float> outputPeaks (int numColumns) const;                  // combined
    std::vector<float> outputPeaks (int numColumns, int channel) const;     // channel < 0 = combined
    // Peak-per-column of the loaded source. channel < 0 = combined (max across channels);
    // otherwise that single channel. Used by the SAMPLE panel's Stereo/Combined views.
    std::vector<float> sourcePeaks (int numColumns, int channel) const;
    int  sourceChannels() const               { return sampleBuffer.getNumChannels(); }
    double dryRegionSeconds() const           { return fileSampleRate > 0.0 ? (prepParams.endFrac - prepParams.startFrac) * fileFrames / fileSampleRate : 0.0; }

    // Background render status (UI shows "redrawing" + repaints when the version changes).
    bool isRenderBusy() const                 { return renderBusy.load(); }
    int  renderVersion() const                { return renderVer.load(); }

    juce::String  fileName() const            { return loadedName; }
    double        sampleRate() const          { return fileSampleRate; }
    int           bitDepth() const            { return fileBits; }
    juce::int64   lengthFrames() const        { return fileFrames; }
    double        lengthSeconds() const       { return fileSampleRate > 0.0 ? regionLen.load() / fileSampleRate : 0.0; }
    double        positionSeconds() const     { return transport.getCurrentPosition(); }

    std::function<void()> onChange;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void requestRender();                                                  // signal the worker
    void renderLoop();                                                     // worker thread body
    void doRender (const PrepParams&, const FxRack&, double tempo,
                   const std::array<Transformer, kNumTransformers>&);      // the actual render
    // Where the retained tail material is copied out of a render. Pristine: captured before the
    // output fades are applied, from just before the cut point to the end of the render window.
    struct TailCapture
    {
        juce::AudioBuffer<float>* buffer = nullptr;
        int startInOutput = 0;      // sample index in the output timeline where `buffer` begins
        int len = 0;                // valid samples captured
    };
    int  renderInto (const PrepParams&, const FxRack&,
                     const std::array<Transformer, kNumTransformers>&, double tempo,
                     juce::AudioBuffer<float>& work,
                     const FxMeterSink* meter = nullptr,
                     TailCapture* tail = nullptr) const;                   // shared render core
    int  renderTailSamples (const FxRack&, double tempo) const;            // full ring-out (always computed)
    int  grantSamples (const TailGrant&) const;                            // capped output extension

    // Undo/redo. Every state-changing setter calls recordEdit() BEFORE it mutates, so the stack
    // holds pre-change snapshots; `tag` identifies the gesture so a drag coalesces to one step.
    struct EditState
    {
        PrepParams prep;
        FxRack rack;
        std::array<Transformer, kNumTransformers> trans;
    };
    struct HistoryStep
    {
        EditState state;
        juce::String desc;      // shown to the user ("Filter Cutoff")
        int tag = 0;            // gesture identity for coalescing (0 = never coalesce)
        juce::uint32 stamp = 0; // when this step was last touched
    };

    EditState captureState() const;
    void applyState (const EditState&);                     // swap state in + re-render (no recording)
    void recordEdit (int tag, const juce::String& desc);
    juce::String slotName (int slot) const;                 // effect name, for step descriptions
    juce::String paramName (int slot, int paramIndex) const;

    static constexpr int kHistoryLimit = 100;               // ~4 KB per step
    static constexpr juce::uint32 kCoalesceMs = 600;        // same-target changes inside this = one step
    std::deque<HistoryStep> undoStack, redoStack;

    // Background render worker — keeps the UI responsive on long (up to 15 s) tail renders.
    struct RenderThread : juce::Thread
    {
        explicit RenderThread (AudioEngine& o) : juce::Thread ("sampleark-render"), owner (o) {}
        void run() override { owner.renderLoop(); }
        AudioEngine& owner;
    };
    RenderThread renderThread { *this };
    juce::WaitableEvent renderSignal;
    juce::CriticalSection stateLock;          // guards prepParams + fxRack vs the worker snapshot
    std::atomic<bool> renderDirty { false };
    std::atomic<bool> renderBusy { false };
    std::atomic<int>  renderVer { 0 };

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;

    juce::AudioBuffer<float> sampleBuffer;                       // decoded source, in RAM
    juce::AudioBuffer<float> playBuffer;                         // prepped one-shot (region + tail)
    juce::AudioBuffer<float> renderTemp;                         // scratch for renderPrep (no per-render alloc)
    // Retained tail: bounded (kTailCapSeconds + margin) and independent of source length, so
    // keeping the computed-but-unpublished ring-out costs a fixed ~2.6 MB rather than a third
    // source-sized buffer. Pristine — the output fade is applied on the way into playBuffer only.
    juce::AudioBuffer<float> tailBuffer, tailTemp;               // published / worker scratch
    std::atomic<int> tailLen { 0 }, tailStart { 0 };             // valid samples / start in the output
    std::unique_ptr<juce::PositionableAudioSource> playSource;   // plays playBuffer

    PrepParams prepParams;
    FxRack fxRack;
    std::array<Transformer, kNumTransformers> transformerArray;
    int selSlot = 5;   // Filter selected by default

    // Per-slot dynamics metering (one frame per kMeterHop samples). slotMeters is published for
    // the UI under playLock; renderMeters is the worker's scratch, swapped in on render completion.
    static constexpr int kMeterHop = 256;
    struct SlotMeter { std::vector<float> outDb, grDb; int frames = 0; };
    std::array<SlotMeter, kNumSlots> slotMeters, renderMeters;

    juce::AudioThumbnailCache thumbCache { 2 };
    juce::AudioThumbnail thumb { 512, formatManager, thumbCache };

    std::atomic<bool> loopOn { false };
    std::atomic<int> regionLen { 0 };   // prepped region length (samples); play/loop bound
    juce::SpinLock playLock;            // guards playBuffer swaps vs the audio thread
    double tempoBpm = 120.0;

    juce::String loadedName;
    double  fileSampleRate = 0.0;
    int     fileBits = 0;
    juce::int64 fileFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
