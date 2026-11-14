#pragma once

#include "sampleark/TimeStretch.h"

namespace sampleark
{
// Warp candidate #1 for the Arch07 P1 bake-off: Signalsmith Stretch 1.4.0 (MIT, vendored in
// third_party/). Spectral, pitch-preserving: tempo and pitch move independently; Formant shifts
// the spectral envelope (with pitch compensation, so 0 = formants stay put while transposing).
//
// Offline over the whole region via its `exact()` call, so the output is exactly the requested
// length and aligned with the input (no pre-roll). A fresh, fixed-seed stretcher per call keeps
// it stateless (safe from the render worker and the variations renderer at once) and
// deterministic — the library otherwise seeds its phase randomiser from std::random_device.
class SignalsmithWarpEngine final : public TimeStretchEngine
{
public:
    const char* name() const override         { return "Signalsmith"; }
    bool preservesPitch() const override      { return true; }
    bool isIdentity (const StretchParams& p) const override;
    int  outputLength (int inLen, const StretchParams& p) const override;
    int  process (const float* const* in, int numChannels, int inLen, double sampleRate,
                  const StretchParams& p, float* const* out, int outCapacity) override;

    static double timeRatio (const StretchParams& p);   // tempoRatio, clamped like Resample's rate
};
}
