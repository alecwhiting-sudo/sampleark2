#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <functional>

// M3 built-in FX rack. 8 reorderable slots, each a built-in effect. Four are implemented
// with real DSP (Distortion, Bitcrush, Delay, Filter); the rest are passthrough until M5.
// Processes a buffer offline (the prepped one-shot) — same code drives preview and export.
namespace sa
{
enum class FxType { Distortion, Bitcrush, Compression, Delay, Reverb, Filter, Limiter, Autopan };
constexpr int kNumSlots = 8;

// Longest tail we will ever compute or publish. Bounds the render window, the retained-tail
// buffer and any manually requested length.
constexpr double kTailCapSeconds = 15.0;

// How much a tail-producing effect (Delay/Reverb) is allowed to extend the rendered sample past
// the trimmed region. The DSP is unaffected either way — this is purely output-length policy, so
// the ring-out is always computed and kept, we only choose where the published sample stops.
//   Off  - never extends; the sample ends at the region. THE DEFAULT: a loop stays loop-length
//          until you ask for a tail, rather than every reverb quietly making the sample longer.
//   Full - the effect's natural ring-out (what the app did before this existed).
//   Time - exactly the dialled time.
//   Mult - exactly the dialled multiple of the region length.
// Time/Mult are exact: short ring-outs are padded rather than trimmed back.
// (A Sync mode — bars/beats at project tempo — belongs after Mult once M6 lands.)
enum class TailMode { Off, Full, Time, Mult };

const char* tailModeName (TailMode);          // "OFF" / "FULL" / "TIME" / "xLOOP"
double tailAmountSeconds  (float amount01);   // TIME  knob -> 0.01 .. 10 s   (log)
double tailAmountMultiple (float amount01);   // xLOOP knob -> 0.05 .. 4.0 x  (log)
bool   fxProducesTail (FxType);               // Delay | Reverb

// A slot's claim on the output length, and whether that claim is an exact length (Time/Mult)
// rather than an estimate that trailing-silence trimming may shorten (Full).
struct TailGrant
{
    double seconds = 0.0;
    bool   exact   = false;
};

struct FxParam
{
    const char* id;
    const char* label;
    float def;
    std::vector<const char*> options = {};   // non-empty -> segmented control (value = index)
    bool isSeg() const { return ! options.empty(); }
};

struct FxInfo
{
    const char* name;
    std::vector<FxParam> params;
    bool implemented;   // has real DSP yet
};

const FxInfo& fxInfo (FxType);

// Effective delay time (seconds) from a Delay slot's params + tempo (free ms or note sync).
double delayTimeSeconds (const std::vector<float>& delayParams, double tempoBpm);

struct FxSlot
{
    FxType type = FxType::Distortion;
    bool on = true;
    std::vector<float> params;   // normalised, sized to fxInfo(type).params

    // Tail policy (Delay/Reverb only; ignored by every other type). Deliberately NOT an FxParam:
    // params get re-rolled by the mutation engine and offered as transformer targets, and a tail
    // length should do neither — variations inherit whatever the user set.
    TailMode tailMode   = TailMode::Off;
    float    tailAmount = 0.566f;   // normalised knob position: ~500 ms / ~x0.60

    FxSlot() = default;
    explicit FxSlot (FxType t, bool isOn = true);
};

// The natural ring-out of one slot (0 when bypassed, dry, or not a tail producer).
double naturalTailSeconds (const FxSlot&, double tempoBpm);
// What that slot is allowed to add to the output, per its tail mode.
TailGrant tailGrant (const FxSlot&, double tempoBpm, double regionSeconds);

// Time-varying modulation supplied by the transformer engine (M3a). Empty functions mean
// "no modulation". Sample position is relative to the render buffer start.
struct FxModulation
{
    std::function<float(int slot, int param, int samplePos)> paramAdd;   // additive offset (continuous params)
    std::function<float(int samplePos)> preGain;                         // amplitude before the rack (×, 1 = unity)
    std::function<float(int samplePos)> postGain;                        // amplitude after the rack
};

// Per-block metering captured during render (for the dynamics editors' OUT + GR meters). Called
// once per active slot per block with that slot's post-effect output peak and gain reduction.
struct FxMeterSink
{
    std::function<void(int slot, int startSample, float outPeak, float grDb)> capture;
};

class FxRack
{
public:
    FxRack();   // default 8-slot layout per Arch04

    std::array<FxSlot, kNumSlots>& slots()             { return slotArray; }
    const std::array<FxSlot, kNumSlots>& slots() const { return slotArray; }

    void toggleBypass (int slot);
    void setTailMode (int slot, TailMode);
    void setTailAmount (int slot, float amount01);

    // Tail lengths across the rack. `naturalTail` is what the DSP needs to ring out fully (mode
    // ignored — always computed, so the material is there for the retained-tail buffer and the
    // features built on it). `outputGrant` is the longest claim on the published length; its
    // `exact` flag is taken from the winning slot.
    double    naturalTail (double tempoBpm) const;
    TailGrant outputGrant (double tempoBpm, double regionSeconds) const;

    void move (int from, int to);
    void setParam (int slot, int paramIndex, float value);

    // Processes block-by-block with persistent per-effect state, so params can move over
    // time (transformers). With an empty FxModulation this is bit-equivalent to static params.
    void process (juce::AudioBuffer<float>& buffer, double sampleRate, double tempoBpm,
                  const FxModulation& mod = {}, const FxMeterSink* meter = nullptr) const;

private:
    std::array<FxSlot, kNumSlots> slotArray;
};
}
