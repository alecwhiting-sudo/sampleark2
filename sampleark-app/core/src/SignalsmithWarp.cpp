#include "SignalsmithWarp.h"

#include "signalsmith-stretch.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sampleark
{
double SignalsmithWarpEngine::timeRatio (const StretchParams& p)
{
    return std::min (ResampleEngine::kMaxRate, std::max (ResampleEngine::kMinRate, p.tempoRatio));
}

bool SignalsmithWarpEngine::isIdentity (const StretchParams& p) const
{
    return std::abs (timeRatio (p) - 1.0) < 1.0e-9 && std::abs (p.pitchSemis) < 1.0e-9
        && std::abs (p.formantSemis) < 1.0e-9;
}

int SignalsmithWarpEngine::outputLength (int inLen, const StretchParams& p) const
{
    if (inLen <= 0) return 0;
    if (isIdentity (p)) return inLen;
    return std::max (1, (int) std::llround ((double) inLen / timeRatio (p)));
}

int SignalsmithWarpEngine::process (const float* const* in, int numChannels, int inLen, double sampleRate,
                                    const StretchParams& p, float* const* out, int outCapacity)
{
    const int outLen = std::min (outputLength (inLen, p), outCapacity);
    if (outLen <= 0 || numChannels <= 0) return 0;
    if (isIdentity (p))
    {
        for (int c = 0; c < numChannels; ++c) std::copy (in[c], in[c] + outLen, out[c]);
        return outLen;
    }

    signalsmith::stretch::SignalsmithStretch<float> st (20260101L);   // fixed seed: deterministic
    st.presetDefault (numChannels, (float) sampleRate);
    st.setTransposeSemitones ((float) p.pitchSemis);
    if (std::abs (p.formantSemis) > 1.0e-9)
        st.setFormantSemitones ((float) p.formantSemis, true);

    // exact() needs at least one seek-length of input. A short one-shot is zero-padded to that,
    // processed, and the first outLen samples kept — the padding only ever lands in the cut-off end.
    const double ratio = timeRatio (p);
    const int seekLen = st.outputSeekLength ((float) ratio);
    const int padIn   = std::max (inLen, seekLen + 1);
    const int padOut  = std::max (outLen, (int) std::llround (padIn / ratio));

    std::vector<std::vector<float>> inBuf ((size_t) numChannels), outBuf ((size_t) numChannels);
    std::vector<const float*> ip ((size_t) numChannels);
    std::vector<float*> op ((size_t) numChannels);
    for (int c = 0; c < numChannels; ++c)
    {
        if (padIn > inLen)
        {
            inBuf[(size_t) c].assign ((size_t) padIn, 0.0f);
            std::copy (in[c], in[c] + inLen, inBuf[(size_t) c].begin());
            ip[(size_t) c] = inBuf[(size_t) c].data();
        }
        else ip[(size_t) c] = in[c];

        if (padOut > outLen) { outBuf[(size_t) c].assign ((size_t) padOut, 0.0f); op[(size_t) c] = outBuf[(size_t) c].data(); }
        else op[(size_t) c] = out[c];
    }

    st.exact (ip.data(), padIn, op.data(), padOut);

    if (padOut > outLen)
        for (int c = 0; c < numChannels; ++c)
            std::copy (op[(size_t) c], op[(size_t) c] + outLen, out[c]);
    return outLen;
}
}
