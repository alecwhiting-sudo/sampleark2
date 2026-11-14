#pragma once

// M6 Tempo & Warp: the swappable time/pitch stage (Arch02 -> Time-Stretch Engine).
//
// Signal chain position (Arch06): Source -> Trim -> Fades -> **Pitch/Tempo** -> Rack -> Output.
//
// The app renders offline on a background worker for preview AND print (preview == print is an
// Arch03 invariant), so an engine is a single offline call over a whole region. There is no
// separate real-time preview path to keep in parity: the same call produces both.
//
// Headless and JUCE-free on purpose, so engines can be unit-tested and bake-off candidates
// (Rubber Band / SoundTouch / Signalsmith) can drop in behind this interface without touching
// the render core.
namespace sampleark
{
// What the user asked for, independent of how an engine achieves it.
struct StretchParams
{
    // Playback-speed factor from tempo sync: session tempo / loop tempo. 1 = unchanged,
    // >1 = faster (shorter). Changes duration in every mode.
    double tempoRatio = 1.0;

    // Pitch shift in semitones (transpose + tune/100). A resampling engine cannot keep this apart
    // from duration — that is the old-school sound. A pitch-preserving engine applies it alone.
    double pitchSemis = 0.0;

    // Formant shift in semitones. Only meaningful to engines that support it.
    double formantSemis = 0.0;
};

class TimeStretchEngine
{
public:
    virtual ~TimeStretchEngine() = default;

    virtual const char* name() const = 0;

    // True if pitch and duration are independent (modern warp). False = speed and pitch linked.
    virtual bool preservesPitch() const = 0;

    // True when `p` is a no-op for this engine (callers may skip the stage entirely — and must
    // still get bit-identical audio if they don't).
    virtual bool isIdentity (const StretchParams& p) const = 0;

    // Output length (samples) for an input of `inLen` samples.
    virtual int outputLength (int inLen, const StretchParams& p) const = 0;

    // Process planar float channels. `out` must hold outputLength() samples per channel; returns
    // the number written. Deterministic: the same input and params always give the same output.
    virtual int process (const float* const* in, int numChannels, int inLen, double sampleRate,
                         const StretchParams& p, float* const* out, int outCapacity) = 0;
};

// Old-school sampler resampling: speed and pitch move together. Band-limited (Kaiser-windowed
// sinc, cutoff follows the rate) so speeding up doesn't alias — the character comes from the
// linked speed/pitch, not from artefacts. A rate of exactly 1 is a straight copy (nulls).
class ResampleEngine final : public TimeStretchEngine
{
public:
    const char* name() const override         { return "Resample"; }
    bool preservesPitch() const override      { return false; }
    bool isIdentity (const StretchParams& p) const override;
    int  outputLength (int inLen, const StretchParams& p) const override;
    int  process (const float* const* in, int numChannels, int inLen, double sampleRate,
                  const StretchParams& p, float* const* out, int outCapacity) override;

    // Combined playback rate for `p` (input samples consumed per output sample), clamped to
    // [kMinRate, kMaxRate]. tempoRatio * 2^(pitchSemis / 12).
    static double rate (const StretchParams& p);

    static constexpr double kMinRate = 0.125;   // three octaves down
    static constexpr double kMaxRate = 8.0;     // three octaves up
};

// Pitch-preserving warp. No engine has passed the quality bar yet (Arch07 P1 bake-off), so this
// is the slot candidates plug into; returns nullptr until one does.
TimeStretchEngine* warpEngine();

// The engine for a time mode: 0 = Warp (pitch-preserving), 1 = Resample. Falls back to
// Resample when no warp engine is available. Engines are stateless between calls.
TimeStretchEngine& engineForMode (int timeMode);
bool warpAvailable();
}
