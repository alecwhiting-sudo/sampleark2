#pragma once

#include <vector>

// M6 tempo detection for loop material (the toolbar DETECT button). The result is always a
// suggestion: the user can edit it, and DETECT again steps through the octave alternatives.
//
// Method: an onset-strength envelope (log-energy flux, full band + a high-passed band so hats
// count as much as kicks) is autocorrelated, and each candidate tempo is scored by a comb over
// its beat multiples with a mild prior towards ~120 BPM. The winner is then *length-locked*:
// a loop is almost always a whole number of bars, so if the region length fits a whole bar
// (or beat) count within a few percent of the estimate, the length-exact tempo wins. That is
// what makes a synced loop stay locked rather than drift.
namespace sampleark
{
struct TempoEstimate
{
    bool   valid = false;       // false = no usable periodicity (a one-shot, or too short)
    double bpm = 0.0;           // best estimate
    double confidence = 0.0;    // 0..1 (peak prominence of the tempo score)
    double beats = 0.0;         // region length in beats at `bpm`
    bool   lengthLocked = false;// bpm was snapped so the region is a whole number of bars/beats
    std::vector<double> alternatives;   // octave-related candidates (x2, /2, x1.5 ...), best first,
                                        // excluding `bpm`; each length-locked where possible
};

struct TempoDetectOptions
{
    double minBpm = 70.0;       // search range for the primary estimate
    double maxBpm = 180.0;
    double altMinBpm = 50.0;    // range alternatives may fall in
    double altMaxBpm = 220.0;
};

// `mono` is the region to analyse (mixed to mono by the caller). Deterministic.
TempoEstimate detectTempo (const float* mono, int numSamples, double sampleRate,
                           const TempoDetectOptions& opts = {});

// Length-only fit: the tempo at which `seconds` is a whole number of bars (4/4) closest to
// `nearBpm`. Used to lock an estimate, and to offer a sane default when detection fails.
double lengthLockedBpm (double seconds, double nearBpm, bool* isWholeBars = nullptr);
}
