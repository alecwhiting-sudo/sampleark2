#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// M3 built-in FX rack. 8 reorderable slots, each a built-in effect. Four are implemented
// with real DSP (Distortion, Bitcrush, Delay, Filter); the rest are passthrough until M5.
// Processes a buffer offline (the prepped one-shot) — same code drives preview and export.
namespace sa
{
enum class FxType { Distortion, Bitcrush, Compression, Delay, Reverb, Filter, Limiter, Autopan };
constexpr int kNumSlots = 8;

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

    FxSlot() = default;
    explicit FxSlot (FxType t, bool isOn = true);
};

class FxRack
{
public:
    FxRack();   // default 8-slot layout per Arch04

    std::array<FxSlot, kNumSlots>& slots()             { return slotArray; }
    const std::array<FxSlot, kNumSlots>& slots() const { return slotArray; }

    void toggleBypass (int slot);
    void move (int from, int to);
    void setParam (int slot, int paramIndex, float value);

    void process (juce::AudioBuffer<float>& buffer, double sampleRate, double tempoBpm) const;

private:
    std::array<FxSlot, kNumSlots> slotArray;
};
}
