// core_tempo_warp_test.cpp — headless checks for the M6 core: ResampleEngine + tempo detection.
// Pure sampleark_core (no JUCE). Built and run by CTest: `ctest --test-dir build-native`.
#include "sampleark/TimeStretch.h"
#include "sampleark/TempoDetect.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace sampleark;

static int failures = 0, checks = 0;
static void check (const char* what, bool ok, double got = NAN)
{
    ++checks;
    if (! ok) ++failures;
    if (std::isnan (got)) std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    else                  std::printf ("%s  %s  (%.4f)\n", ok ? "PASS" : "FAIL", what, got);
}

static constexpr double kSr = 48000.0;
static constexpr double kPi = 3.14159265358979323846;

static std::vector<float> sine (double hz, double secs, double amp = 0.5)
{
    std::vector<float> v ((size_t) (secs * kSr));
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float) (amp * std::sin (2.0 * kPi * hz * (double) i / kSr));
    return v;
}

// Goertzel magnitude of `hz` over [a, b), normalised so a full-scale sine reads ~amplitude.
static double tone (const std::vector<float>& x, double hz, size_t a, size_t b)
{
    const double w = 2.0 * kPi * hz / kSr, c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / (double) (b - a);
}

static std::vector<float> runResample (const std::vector<float>& in, StretchParams p)
{
    ResampleEngine e;
    std::vector<float> out ((size_t) e.outputLength ((int) in.size(), p));
    const float* ip[] = { in.data() };
    float* op[] = { out.data() };
    const int n = e.process (ip, 1, (int) in.size(), kSr, p, op, (int) out.size());
    out.resize ((size_t) n);
    return out;
}

// Drum-ish loop: kick on 1 and 3 ("and" of 2 sometimes), snare on 2 and 4, hats on 8ths.
// Deterministic noise so the test is repeatable.
static std::vector<float> drumLoop (double bpm, int bars, bool syncopated)
{
    const double beat = 60.0 / bpm;
    std::vector<float> v ((size_t) std::llround (bars * 4 * beat * kSr), 0.0f);
    uint32_t rng = 12345;
    auto noise = [&rng] { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) / 8388608.0 - 1.0); };
    auto hit = [&] (double t, int kind)
    {
        const size_t s = (size_t) (t * kSr);
        const size_t len = (size_t) ((kind == 0 ? 0.25 : kind == 1 ? 0.18 : 0.04) * kSr);
        for (size_t i = 0; i < len && s + i < v.size(); ++i)
        {
            const double tt = (double) i / kSr;
            double y;
            if (kind == 0)      y = 0.9 * std::sin (2.0 * kPi * (50.0 + 90.0 * std::exp (-tt * 30.0)) * tt) * std::exp (-tt * 12.0);
            else if (kind == 1) y = 0.6 * (noise() * 0.7 + 0.3 * std::sin (2.0 * kPi * 190.0 * tt)) * std::exp (-tt * 22.0);
            else                y = 0.25 * noise() * std::exp (-tt * 90.0);
            v[s + i] += (float) y;
        }
    };
    for (int b = 0; b < bars; ++b)
    {
        const double t0 = b * 4 * beat;
        hit (t0, 0); hit (t0 + 2 * beat, 0);
        if (syncopated) hit (t0 + 2.5 * beat, 0);
        hit (t0 + beat, 1); hit (t0 + 3 * beat, 1);
        for (int e = 0; e < 8; ++e) hit (t0 + e * beat * 0.5, 2);
    }
    return v;
}

static bool octaveMatch (double got, double want, double tol)
{
    for (double f : { 1.0, 2.0, 0.5 })
        if (std::abs (got - want * f) <= tol * f) return true;
    return false;
}

int main()
{
    // ---------- ResampleEngine ----------
    {
        ResampleEngine e;
        StretchParams id;
        check ("identity params report identity", e.isIdentity (id));
        const auto in = sine (440.0, 0.5);
        const auto out = runResample (in, id);
        bool same = out.size() == in.size();
        for (size_t i = 0; same && i < in.size(); ++i) same = (out[i] == in[i]);
        check ("rate 1 is a bit-exact copy (nulls)", same);
    }
    {
        StretchParams up; up.pitchSemis = 12.0;
        check ("+12 semis = rate 2", std::abs (ResampleEngine::rate (up) - 2.0) < 1e-12, ResampleEngine::rate (up));
        const auto in = sine (440.0, 1.0);
        const auto out = runResample (in, up);
        check ("+12 semis halves the length", out.size() == in.size() / 2, (double) out.size());
        const double at880 = tone (out, 880.0, 2000, out.size() - 2000);
        const double at440 = tone (out, 440.0, 2000, out.size() - 2000);
        check ("+12 semis moves 440 Hz to 880 Hz", at880 > 0.45 && at440 < 0.01, at880);
    }
    {
        StretchParams dn; dn.pitchSemis = -7.0;
        const auto in = sine (1000.0, 1.0);
        const auto out = runResample (in, dn);
        const double expectHz = 1000.0 * std::pow (2.0, -7.0 / 12.0);
        const double a = tone (out, expectHz, 4000, out.size() - 4000);
        check ("-7 semis lands on the right frequency at unity gain", std::abs (a - 0.5) < 0.01, a);
        check ("-7 semis lengthens by 2^(7/12)",
               std::abs ((double) out.size() / in.size() - std::pow (2.0, 7.0 / 12.0)) < 1e-3,
               (double) out.size() / in.size());
    }
    {
        // Speeding up must band-limit: a 15 kHz tone at rate 2 would fold to 18 kHz alias if not.
        StretchParams up; up.pitchSemis = 12.0;
        const auto in = sine (15000.0, 0.5);
        const auto out = runResample (in, up);
        double peak = 0.0;
        for (size_t i = 2000; i + 2000 < out.size(); ++i) peak = std::max (peak, (double) std::abs (out[i]));
        check ("speeding up removes content above the new Nyquist (no aliasing)", peak < 0.005, peak);
    }
    {
        StretchParams t; t.tempoRatio = 174.0 / 136.0;
        const auto in = sine (200.0, 2.0);
        const auto out = runResample (in, t);
        check ("tempo ratio sets length (136 -> 174 BPM)",
               std::abs ((double) out.size() - in.size() * 136.0 / 174.0) <= 1.0, (double) out.size());
    }
    {
        StretchParams wild; wild.pitchSemis = 60.0;
        check ("rate is clamped to the supported range", ResampleEngine::rate (wild) == ResampleEngine::kMaxRate);
    }
    {
        StretchParams p; p.pitchSemis = 3.3;
        const auto in = drumLoop (120.0, 1, false);
        const auto a = runResample (in, p), b = runResample (in, p);
        check ("resampling is deterministic", a == b);
    }
    check ("Resample mode always available", std::string (engineForMode (1).name()) == "Resample");
    check ("Warp falls back to Resample while no warp engine ships",
           warpAvailable() || std::string (engineForMode (0).name()) == "Resample");

    // ---------- tempo detection ----------
    struct Case { double bpm; int bars; bool sync; };
    for (const Case c : { Case { 120.0, 2, false }, Case { 136.0, 4, true }, Case { 174.0, 4, true },
                          Case { 90.0, 2, true }, Case { 128.0, 1, false } })
    {
        const auto loop = drumLoop (c.bpm, c.bars, c.sync);
        const TempoEstimate e = detectTempo (loop.data(), (int) loop.size(), kSr);
        char what[128];
        std::snprintf (what, sizeof what, "detects %.0f BPM (%d bars) up to octave", c.bpm, c.bars);
        check (what, e.valid && octaveMatch (e.bpm, c.bpm, 0.05), e.bpm);
        bool exact = e.valid && (std::abs (e.bpm - c.bpm) < 0.05
                                 || [&] { for (double a : e.alternatives) if (std::abs (a - c.bpm) < 0.05) return true; return false; }());
        std::snprintf (what, sizeof what, "  %.0f BPM exact, as primary or an alternative", c.bpm);
        check (what, exact);
        std::snprintf (what, sizeof what, "  %.0f BPM estimate is length-locked", c.bpm);
        check (what, e.lengthLocked);
    }
    {
        // A one-shot: a single decaying hit. Nothing periodic to find.
        std::vector<float> v ((size_t) (1.5 * kSr), 0.0f);
        for (size_t i = 0; i < (size_t) (0.4 * kSr); ++i)
        {
            const double t = (double) i / kSr;
            v[i] = (float) (0.9 * std::sin (2.0 * kPi * 60.0 * t) * std::exp (-t * 8.0));
        }
        const TempoEstimate e = detectTempo (v.data(), (int) v.size(), kSr);
        check ("a one-shot reports no tempo", ! e.valid, e.confidence);
    }
    {
        const std::vector<float> silent ((size_t) kSr * 2, 0.0f);
        check ("silence reports no tempo", ! detectTempo (silent.data(), (int) silent.size(), kSr).valid);
    }
    {
        bool bars = false;
        const double secs = 4 * 4 * 60.0 / 136.0;
        const double l = lengthLockedBpm (secs, 135.2, &bars);
        check ("length lock snaps 135.2 to 136 for a 4-bar loop", std::abs (l - 136.0) < 1e-9 && bars, l);
        check ("length lock refuses a poor fit", lengthLockedBpm (secs, 150.0) == 0.0);
    }

    std::printf ("\n%d / %d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
