#include "sampleark/TempoDetect.h"

#include <algorithm>
#include <cmath>

namespace sampleark
{
namespace
{
// Prior: electronic material centres a little above 120. Broad (about an octave either side),
// so the signal decides and the prior only breaks octave ties.
constexpr double kPriorCentre = 128.0;
constexpr double kPriorWidth  = 0.9;     // in octaves (one sigma)

double prior (double bpm)
{
    const double o = std::log2 (bpm / kPriorCentre) / kPriorWidth;
    return std::exp (-0.5 * o * o);
}

// Onset strength per hop: half-wave rectified flux of compressed energy, full band plus a
// first-difference (high-passed) band so hats count alongside kicks. The analysis window is
// ~23 ms — longer than a bass cycle, so a low sine's own ripple never reads as onsets — and the
// energy is peak-normalised and log1p-compressed so quiet noise-floor jitter contributes nothing.
std::vector<float> onsetEnvelope (const float* x, int n, int hop, double fps, double sr)
{
    const int win = std::max (hop * 2, (int) std::lround (0.023 * sr));
    const int frames = n >= win ? 1 + (n - win) / hop : 0;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) peak = std::max (peak, std::abs (x[i]));
    const double norm = peak > 0.0f ? 1.0 / ((double) peak * peak * win) : 0.0;

    // Hann-weighted (sums to ~win/2): suppresses the 2x-frequency ripple of low tones' energy.
    std::vector<double> hann ((size_t) win);
    for (int i = 0; i < win; ++i) hann[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (i + 0.5) / win);

    std::vector<float> lf ((size_t) frames), lh ((size_t) frames), o ((size_t) frames, 0.0f);
    for (int f = 0; f < frames; ++f)
    {
        double ef = 0.0, eh = 0.0;
        const int a = f * hop;
        for (int i = a; i < a + win; ++i)
        {
            const double v = x[i];
            const double d = v - (i > 0 ? x[i - 1] : 0.0);
            const double h = hann[(size_t) (i - a)];
            ef += h * v * v;
            eh += h * d * d;
        }
        lf[(size_t) f] = (float) std::log1p (200.0 * ef * norm);
        lh[(size_t) f] = (float) std::log1p (200.0 * eh * norm);
    }
    // The region is treated as starting from silence, so a hit right on sample 0 (the usual case
    // for a trimmed loop) counts as the onset it is.
    for (int f = 0; f < frames; ++f)
    {
        const float pf = f > 0 ? lf[(size_t) f - 1] : 0.0f, ph = f > 0 ? lh[(size_t) f - 1] : 0.0f;
        o[(size_t) f] = std::max (0.0f, lf[(size_t) f] - pf) + std::max (0.0f, lh[(size_t) f] - ph);
    }

    // Subtract a ~0.2 s moving average, rectify: leaves the peaks.
    std::vector<float> out ((size_t) frames, 0.0f);
    const int span = std::max (2, (int) std::lround (0.1 * fps));     // frames either side (~0.1 s)
    double acc = 0.0; int cnt = 0;
    int lo = 0, hi = -1;
    for (int f = 0; f < frames; ++f)
    {
        while (hi < std::min (frames - 1, f + span)) { ++hi; acc += o[(size_t) hi]; ++cnt; }
        while (lo < f - span)                        { acc -= o[(size_t) lo]; ++lo; --cnt; }
        const double mean = cnt > 0 ? acc / cnt : 0.0;
        out[(size_t) f] = (float) std::max (0.0, (double) o[(size_t) f] - mean);
    }
    return out;
}

struct Scorer
{
    std::vector<double> r;      // autocorrelation by integer lag
    double framesPerSec = 0.0;

    double acAt (double lag) const
    {
        if (lag < 1.0) return 0.0;
        const int i = (int) lag;
        if (i + 1 >= (int) r.size()) return 0.0;
        const double f = lag - i;
        return r[(size_t) i] * (1.0 - f) + r[(size_t) i + 1] * f;
    }

    // Comb over the beat and its multiples, plus the half-beat (an 8th-note hat line supports
    // its own tempo, not one a third away). Only lags inside half the analysed length count, so
    // a short loop isn't scored on two frames of overlap.
    double score (double bpm) const
    {
        const double L = 60.0 * framesPerSec / bpm;
        static const double mult[] = { 0.5, 1.0, 2.0, 3.0, 4.0 };
        static const double w[]    = { 0.5, 1.0, 0.5, 0.33, 0.25 };
        double s = 0.0, ws = 0.0;
        for (int m = 0; m < 5; ++m)
        {
            const double lag = L * mult[m];
            if (lag > maxUsableLag) break;
            s += w[m] * acAt (lag);
            ws += w[m];
        }
        return ws > 0.0 ? s / ws : 0.0;
    }
    double maxUsableLag = 0.0;
};
}

double lengthLockedBpm (double seconds, double nearBpm, bool* isWholeBars)
{
    if (isWholeBars != nullptr) *isWholeBars = false;
    if (seconds <= 0.0 || nearBpm <= 0.0) return 0.0;

    const double beats = seconds * nearBpm / 60.0;
    const double nb = std::round (beats / 4.0);
    if (nb >= 1.0)
    {
        const double cand = 240.0 * nb / seconds;
        const double tol = std::min (0.04, 0.4 / nb);          // adjacent bar counts stay distinguishable
        if (std::abs (cand - nearBpm) / nearBpm < tol)
        {
            if (isWholeBars != nullptr) *isWholeBars = true;
            return cand;
        }
    }
    const double nbeat = std::round (beats);
    if (nbeat >= 2.0)
    {
        const double cand = 60.0 * nbeat / seconds;
        if (std::abs (cand - nearBpm) / nearBpm < std::min (0.02, 0.3 / nbeat))
            return cand;
    }
    return 0.0;
}

TempoEstimate detectTempo (const float* mono, int n, double sr, const TempoDetectOptions& opts)
{
    TempoEstimate est;
    if (mono == nullptr || n <= 0 || sr <= 0.0) return est;

    const int hop = std::max (1, (int) std::lround (sr * 0.003));          // ~333 frames / s
    const double fps = sr / hop;
    const double seconds = n / sr;
    if (seconds < 2.0 * 60.0 / opts.maxBpm + 0.1) return est;               // needs a couple of beats

    const std::vector<float> o = onsetEnvelope (mono, n, hop, fps, sr);
    const int frames = (int) o.size();
    double energy = 0.0;
    for (float v : o) energy += (double) v * v;
    if (frames < 8 || energy <= 1.0e-9) return est;

    Scorer sc;
    sc.framesPerSec = fps;
    const int maxLag = std::min (frames - 1, (int) std::ceil (4.0 * 60.0 * fps / opts.altMinBpm) + 2);
    sc.r.assign ((size_t) maxLag + 2, 0.0);
    sc.maxUsableLag = std::min ((double) maxLag, frames * 0.5);
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double s = 0.0;
        for (int f = 0; f + lag < frames; ++f) s += (double) o[(size_t) f] * o[(size_t) f + lag];
        sc.r[(size_t) lag] = s / ((double) (frames - lag) * energy / frames);   // ~1 at perfect periodicity
    }

    // A one-shot has one or two onsets; a loop has many. Count distinct peaks (>= 20% of the
    // strongest, at least 60 ms apart) before trusting any periodicity.
    {
        const float top = *std::max_element (o.begin(), o.end());
        const int gap = (int) std::lround (0.06 * fps);
        int peaks = 0, last = -gap;
        for (int f = 0; f + 1 < frames; ++f)
            if (o[(size_t) f] >= 0.2f * top && (f == 0 || o[(size_t) f] >= o[(size_t) f - 1]) && o[(size_t) f] > o[(size_t) f + 1]
                && f - last >= gap)
            { ++peaks; last = f; }
        if (peaks < 4) return est;
    }

    // Primary: best prior-weighted score inside the primary range.
    constexpr double kStep = 0.05;
    double best = -1.0, bestBpm = 0.0;
    std::vector<double> scores;
    for (double b = opts.minBpm; b <= opts.maxBpm + 1.0e-9; b += kStep)
    {
        const double s = sc.score (b) * prior (b);
        scores.push_back (s);
        if (s > best) { best = s; bestBpm = b; }
    }
    if (best <= 0.0 || scores.empty()) return est;

    std::vector<double> sorted = scores;
    std::nth_element (sorted.begin(), sorted.begin() + (long) sorted.size() / 2, sorted.end());
    const double median = sorted[sorted.size() / 2];
    est.confidence = std::max (0.0, std::min (1.0, (best - median) / best));

    // A single hit (a one-shot) gives a flat score: no tempo to report.
    if (est.confidence < 0.3) return est;

    auto finalise = [seconds] (double bpm, bool* locked)
    {
        bool bars = false;
        const double l = lengthLockedBpm (seconds, bpm, &bars);
        if (locked != nullptr) *locked = l > 0.0;
        return l > 0.0 ? l : std::round (bpm * 10.0) / 10.0;
    };

    est.valid = true;
    est.bpm = finalise (bestBpm, &est.lengthLocked);
    est.beats = seconds * est.bpm / 60.0;

    // Octave-related alternatives, ordered by how well the signal supports them.
    struct Alt { double bpm, s; };
    std::vector<Alt> alts;
    for (double f : { 2.0, 0.5, 1.5, 1.0 / 1.5 })
    {
        const double b = bestBpm * f;
        if (b < opts.altMinBpm || b > opts.altMaxBpm) continue;
        // re-peak locally: the multiple of a quantised estimate can sit a step off its own peak
        double bb = b, bs = -1.0;
        for (double d = -1.5; d <= 1.5; d += kStep)
        {
            const double s = sc.score (b + d) * prior (b + d);
            if (s > bs) { bs = s; bb = b + d; }
        }
        alts.push_back ({ finalise (bb, nullptr), bs });
    }
    std::sort (alts.begin(), alts.end(), [] (const Alt& a, const Alt& b) { return a.s > b.s; });
    for (const auto& a : alts)
        if (std::abs (a.bpm - est.bpm) > 0.5) est.alternatives.push_back (a.bpm);
    return est;
}
}
