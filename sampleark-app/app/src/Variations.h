#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioEngine.h"   // PrepParams, FxRack, Transformer, kNumTransformers
#include <array>
#include <vector>

// M4 variations: a candidate is a full (prep + rack + transformer) state plus its rendered
// audio. The mutation engine perturbs a base state within ranges scaled by a severity level
// and limited to the chosen scopes.
namespace sa
{
// Order matches the MUTATE strip's AFFECTS chips.
enum class Scope { Everything, Plugins, Filter, Envelopes, Dynamics, Crossfades, Timing, Pitch, Mangle, Tail, Count };
using ScopeMask = std::array<bool, (size_t) Scope::Count>;

struct Variation
{
    PrepParams prep;
    FxRack     rack;
    std::array<Transformer, kNumTransformers> trans;

    juce::uint32 seed = 0;
    juce::String name;
    int  number = 0;          // stable display number (00 = baseline); not the row index
    bool selected = false;    // favourited (marked for WRITE)
    bool muted = false;
    bool baseline = false;    // row 0: the unvaried state the user set (the recall anchor / undo)
    bool manual = false;      // captured by the user (+ ADD THIS) rather than mutated

    juce::AudioBuffer<float> audio;     // rendered candidate (stereo)
    int len = 0;                        // valid samples in `audio`
    std::vector<float> peaks;           // combined per-column peaks for the row mini-waveform
};

// Fill `v` as a mutation of `base`: copies the state, then perturbs in-scope parameters by an
// amount scaled by level01 (0 = barely, 1 = unsafe). Deterministic for a given seed.
void mutate (Variation& v, const Variation& base, juce::uint32 seed, float level01, const ScopeMask& scope);

const char* scopeName (Scope s);
const char* depthZoneName (float level01);   // "Gentle" / "Vibing" / "Massive" / "Unsafe"
}
