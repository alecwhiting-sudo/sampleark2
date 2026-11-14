#include "sampleark/TimeStretch.h"

#if SAMPLEARK_WARP_SIGNALSMITH
#include "SignalsmithWarp.h"
#endif

#include <algorithm>
#include <cmath>
#include <vector>

namespace sampleark
{
namespace
{
// Kaiser-windowed sinc, tabulated once. Indexed by distance in zero-crossings (0..kZeroX).
constexpr int    kZeroX      = 16;       // zero crossings each side: ~0.1 dB passband to ~0.95 Nyquist
constexpr int    kOversample = 1024;     // table points per zero crossing (linear-interpolated)
constexpr double kBeta       = 8.0;      // Kaiser beta: ~80 dB stopband
constexpr double kPi         = 3.14159265358979323846;

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x * 0.25;
    for (int k = 1; k < 64; ++k)
    {
        term *= q / ((double) k * (double) k);
        sum += term;
        if (term < sum * 1.0e-12) break;
    }
    return sum;
}

const std::vector<float>& kernelTable()
{
    static const std::vector<float> table = []
    {
        std::vector<float> t ((size_t) (kZeroX * kOversample + 2), 0.0f);
        const double i0b = besselI0 (kBeta);
        for (int i = 0; i <= kZeroX * kOversample; ++i)
        {
            const double u = (double) i / kOversample;                  // zero crossings from centre
            const double sinc = (u == 0.0) ? 1.0 : std::sin (kPi * u) / (kPi * u);
            const double r = u / kZeroX;                                // 0..1 across the window
            const double w = besselI0 (kBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            t[(size_t) i] = (float) (sinc * w);
        }
        return t;                                                       // last slot stays 0 (guard)
    }();
    return table;
}

inline float kernelAt (const std::vector<float>& tab, double u)    // u >= 0, in zero crossings
{
    const double p = u * kOversample;
    const int i = (int) p;
    if (i >= kZeroX * kOversample) return 0.0f;
    const float f = (float) (p - i);
    return tab[(size_t) i] + (tab[(size_t) i + 1] - tab[(size_t) i]) * f;
}
}

double ResampleEngine::rate (const StretchParams& p)
{
    const double r = std::max (1.0e-6, p.tempoRatio) * std::pow (2.0, p.pitchSemis / 12.0);
    return std::min (kMaxRate, std::max (kMinRate, r));
}

bool ResampleEngine::isIdentity (const StretchParams& p) const
{
    return std::abs (rate (p) - 1.0) < 1.0e-9;
}

int ResampleEngine::outputLength (int inLen, const StretchParams& p) const
{
    if (inLen <= 0) return 0;
    if (isIdentity (p)) return inLen;
    return std::max (1, (int) std::llround ((double) inLen / rate (p)));
}

int ResampleEngine::process (const float* const* in, int numChannels, int inLen, double /*sampleRate*/,
                             const StretchParams& p, float* const* out, int outCapacity)
{
    const int outLen = std::min (outputLength (inLen, p), outCapacity);
    if (outLen <= 0 || numChannels <= 0) return 0;

    if (isIdentity (p))
    {
        for (int c = 0; c < numChannels; ++c)
            std::copy (in[c], in[c] + outLen, out[c]);
        return outLen;
    }

    const double r  = rate (p);
    // Cutoff relative to the input Nyquist. Slowing down (r < 1) can't alias, so pass the full
    // band; speeding up moves input frequency f to f*r, so everything above Nyquist/r must go
    // first — with a little margin so the transition band sits below the fold.
    const double fc    = (r <= 1.0) ? 1.0 : 0.97 / r;
    const double halfW = kZeroX / fc;                                   // window half-width, input samples
    const auto&  tab   = kernelTable();

    std::vector<float> w;
    w.reserve ((size_t) (2.0 * halfW) + 4);

    for (int n = 0; n < outLen; ++n)
    {
        const double t  = (double) n * r;                               // input position (exact, no drift)
        const int    k0 = (int) std::ceil (t - halfW);
        const int    k1 = (int) std::floor (t + halfW);

        // Weights over the WHOLE window, so the normaliser is the same at the edges as in the
        // middle (zero-padding then fades the edges in rather than boosting them).
        w.clear();
        double sum = 0.0;
        for (int k = k0; k <= k1; ++k)
        {
            const float h = kernelAt (tab, std::abs (t - (double) k) * fc);
            w.push_back (h);
            sum += h;
        }
        const float norm = sum > 1.0e-12 ? (float) (1.0 / sum) : 0.0f;  // exact unity gain at DC

        const int a = std::max (k0, 0), b = std::min (k1, inLen - 1);
        for (int c = 0; c < numChannels; ++c)
        {
            const float* x = in[c];
            float acc = 0.0f;
            for (int k = a; k <= b; ++k)
                acc += x[k] * w[(size_t) (k - k0)];
            out[c][n] = acc * norm;
        }
    }
    return outLen;
}

// The bake-off slot: which pitch-preserving engine (if any) Warp uses is chosen at build time
// (CMake option SAMPLEARK_WARP), so candidates can be compared without touching the render core.
TimeStretchEngine* warpEngine()
{
#if SAMPLEARK_WARP_SIGNALSMITH
    static SignalsmithWarpEngine signalsmith;
    return &signalsmith;
#else
    return nullptr;
#endif
}

bool warpAvailable()
{
    return warpEngine() != nullptr;
}

TimeStretchEngine& engineForMode (int timeMode)
{
    static ResampleEngine resample;
    if (timeMode == 0)
        if (auto* w = warpEngine())
            return *w;
    return resample;
}
}
