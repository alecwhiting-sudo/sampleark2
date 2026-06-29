#include "FxRack.h"
#include <cmath>

namespace sa
{
const FxInfo& fxInfo (FxType t)
{
    static const FxInfo infos[] = {
        { "Distortion",  {{"drive","Drive",0.55f},{"tone","Tone",0.5f},{"mix","Mix",0.7f}}, true },
        { "Bitcrush",    {{"bits","Bits",0.45f},{"rate","Rate",0.5f},{"mix","Mix",0.6f}}, true },
        { "Compression", {{"thr","Thresh",0.45f},{"ratio","Ratio",0.55f},{"atk","Attack",0.3f},{"rel","Release",0.5f}}, false },
        { "Delay",       {{"time","Time",0.4f},{"fb","Feedbk",0.45f},{"lp","LP",0.7f},{"hp","HP",0.15f},{"mix","Mix",0.3f},{"sync","Sync",0.0f,{"Free","1/4","1/8","1/8.","1/16"}},{"mode","Mode",0.0f,{"Mono","Ping","Mid"}}}, true },
        { "Reverb",      {{"size","Size",0.6f},{"decay","Decay",0.5f},{"mix","Mix",0.28f}}, false },
        { "Filter",      {{"cut","Cutoff",0.66f},{"res","Reso",0.42f},{"env","Env",0.0f},{"type","Type",0.0f,{"LP","BP","HP"}}}, true },
        { "Limiter",     {{"ceil","Ceiling",0.85f},{"rel","Release",0.4f}}, false },
        { "Autopan",     {{"rate","Rate",0.5f},{"depth","Depth",0.45f},{"phase","Phase",0.0f}}, false },
    };
    return infos[(int) t];
}

FxSlot::FxSlot (FxType t, bool isOn) : type (t), on (isOn)
{
    for (auto& pr : fxInfo (t).params)
        params.push_back (pr.def);
}

FxRack::FxRack()
{
    slotArray[0] = FxSlot (FxType::Distortion,  true);
    slotArray[1] = FxSlot (FxType::Bitcrush,    true);
    slotArray[2] = FxSlot (FxType::Compression, false);
    slotArray[3] = FxSlot (FxType::Delay,       true);
    slotArray[4] = FxSlot (FxType::Reverb,      true);
    slotArray[5] = FxSlot (FxType::Filter,      true);
    slotArray[6] = FxSlot (FxType::Limiter,     true);
    slotArray[7] = FxSlot (FxType::Autopan,     false);
}

void FxRack::toggleBypass (int slot)
{
    if (juce::isPositiveAndBelow (slot, kNumSlots))
        slotArray[slot].on = ! slotArray[slot].on;
}

void FxRack::move (int from, int to)
{
    from = juce::jlimit (0, kNumSlots - 1, from);
    to   = juce::jlimit (0, kNumSlots - 1, to);
    if (from == to) return;

    FxSlot moved = slotArray[from];
    if (from < to) for (int i = from; i < to; ++i) slotArray[i] = slotArray[i + 1];
    else           for (int i = from; i > to; --i) slotArray[i] = slotArray[i - 1];
    slotArray[to] = moved;
}

void FxRack::setParam (int slot, int paramIndex, float value)
{
    if (juce::isPositiveAndBelow (slot, kNumSlots)
        && juce::isPositiveAndBelow (paramIndex, (int) slotArray[slot].params.size()))
        slotArray[slot].params[paramIndex] = value;
}

double delayTimeSeconds (const std::vector<float>& p, double tempoBpm)
{
    const float time = p.empty() ? 0.4f : juce::jlimit (0.0f, 1.0f, p[0]);
    const int sync = (int) std::round (p.size() > 5 ? p[5] : 0.0f);
    if (sync <= 0)
        return 0.001 * (1.0 + time * 999.0);                 // free: 1..1000 ms
    const double beat = 60.0 / juce::jmax (1.0, tempoBpm);   // 1/4 note in seconds
    const double beats = (sync == 1) ? 1.0 : (sync == 2) ? 0.5 : (sync == 3) ? 0.75 : 0.25;
    return beats * beat;                                     // 1/4, 1/8, 1/8., 1/16
}

// ---------------- effect DSP (offline, whole-buffer) ----------------
namespace
{
void applyFilter (juce::AudioBuffer<float>& buf, double sr, const std::vector<float>& p)
{
    const double cut = juce::jlimit (0.0f, 1.0f, p[0]);
    const double res = juce::jlimit (0.0f, 1.0f, p[1]);
    const float  env = juce::jlimit (0.0f, 1.0f, p.size() > 2 ? p[2] : 0.0f);  // follower depth
    const int type = (int) std::round (p.size() > 3 ? p[3] : 0.0f);           // 0 LP, 1 BP, 2 HP

    const double baseFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (1000.0, cut));
    const double Q = 0.5 + res * 9.5;
    const double k = 1.0 / Q;

    // envelope follower (fast attack, slower release) drives cutoff up by up to ~4 octaves
    const float aAtk = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
    const float aRel = (float) (1.0 - std::exp (-1.0 / (0.080 * sr)));

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        double ic1 = 0.0, ic2 = 0.0;
        float envf = 0.0f;
        for (int n = 0; n < buf.getNumSamples(); ++n)
        {
            const double x = d[n];

            const float rect = std::abs ((float) x);
            envf += (rect > envf ? aAtk : aRel) * (rect - envf);

            double fc = baseFc * std::pow (2.0, (double) (env * 4.0f * envf));
            fc = juce::jlimit (20.0, sr * 0.45, fc);
            const double g = std::tan (juce::MathConstants<double>::pi * fc / sr);
            const double a1 = 1.0 / (1.0 + g * (g + k));
            const double a2 = g * a1;
            const double a3 = g * a2;

            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            const double out = (type == 0) ? v2 : (type == 2 ? (x - k * v1 - v2) : v1);
            d[n] = (float) out;
        }
    }
}

void applyDistortion (juce::AudioBuffer<float>& buf, const std::vector<float>& p)
{
    const float drive = juce::jlimit (0.0f, 1.0f, p[0]);
    const float tone  = juce::jlimit (0.0f, 1.0f, p[1]);
    const float mix   = juce::jlimit (0.0f, 1.0f, p[2]);
    const double k = 1.0 + drive * 24.0;
    const double nk = std::tanh (k);
    const float a = 0.05f + tone * 0.95f;   // tone = post one-pole brightness

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        float y = 0.0f;
        for (int n = 0; n < buf.getNumSamples(); ++n)
        {
            const float x = d[n];
            const float wet = (float) (std::tanh (x * k) / nk);
            y += a * (wet - y);
            d[n] = x * (1.0f - mix) + y * mix;
        }
    }
}

void applyBitcrush (juce::AudioBuffer<float>& buf, const std::vector<float>& p)
{
    const float bits = juce::jlimit (0.0f, 1.0f, p[0]);
    const float rate = juce::jlimit (0.0f, 1.0f, p[1]);
    const float mix  = juce::jlimit (0.0f, 1.0f, p[2]);
    const double levels = std::pow (2.0, 1.0 + bits * 15.0);
    const int hold = 1 + (int) std::round ((1.0f - rate) * 31.0f);

    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        float held = 0.0f;
        for (int n = 0; n < buf.getNumSamples(); ++n)
        {
            const float x = d[n];
            if (n % hold == 0)
                held = (float) (std::round ((x * 0.5 + 0.5) * (levels - 1.0)) / (levels - 1.0) * 2.0 - 1.0);
            d[n] = x * (1.0f - mix) + held * mix;
        }
    }
}

void applyDelay (juce::AudioBuffer<float>& buf, double sr, const std::vector<float>& p, double tempo)
{
    const float fb   = juce::jlimit (0.0f, 1.0f, p[1]) * 0.95f;
    const float lp   = juce::jlimit (0.0f, 1.0f, p.size() > 2 ? p[2] : 1.0f);
    const float hp   = juce::jlimit (0.0f, 1.0f, p.size() > 3 ? p[3] : 0.0f);
    const float mix  = juce::jlimit (0.0f, 1.0f, p.size() > 4 ? p[4] : 0.3f);
    const int ds = juce::jmax (1, (int) (sa::delayTimeSeconds (p, tempo) * sr));

    // One-pole coefficients for the feedback-path LP/HP (each repeat is shaped).
    // 2-pole resonant SVFs (12 dB/oct + a little resonance at the corner) for real bite.
    const double pi = juce::MathConstants<double>::pi;
    const double lpFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (1000.0, (double) lp));
    const double hpFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (500.0,  (double) hp));
    const double kQ = 1.0 / 1.3;   // Q ~ 1.3
    const double gL = std::tan (pi * lpFc / sr), a1L = 1.0 / (1.0 + gL * (gL + kQ)), a2L = gL * a1L, a3L = gL * a2L;
    const double gH = std::tan (pi * hpFc / sr), a1H = 1.0 / (1.0 + gH * (gH + kQ)), a2H = gH * a1H, a3H = gH * a2H;
    const bool lpActive = (lp < 0.999f);   // skip when wide open
    const bool hpActive = (hp > 0.001f);
    const int mode = (int) std::round (p.size() > 6 ? p[6] : 0.0f);   // 0 Mono, 1 Ping, 2 Mid

    // Shape a delayed sample through the (per-line) 2-pole LP then HP.
    auto svf = [&] (double s, double& l1, double& l2, double& h1, double& h2)
    {
        if (lpActive)
        {
            const double v3 = s - l2, v1 = a1L * l1 + a2L * v3, v2 = l2 + a2L * l1 + a3L * v3;
            l1 = 2.0 * v1 - l1; l2 = 2.0 * v2 - l2; s = v2;
        }
        if (hpActive)
        {
            const double w3 = s - h2, w1 = a1H * h1 + a2H * w3, w2 = h2 + a2H * h1 + a3H * w3;
            h1 = 2.0 * w1 - h1; h2 = 2.0 * w2 - h2; s = s - kQ * w1 - w2;
        }
        return s;
    };

    if (mode == 0 || buf.getNumChannels() < 2)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            std::vector<float> line ((size_t) ds, 0.0f);
            int wp = 0;
            double l1 = 0, l2 = 0, h1 = 0, h2 = 0;
            for (int n = 0; n < buf.getNumSamples(); ++n)
            {
                const float x = d[n];
                const float filtered = (float) svf (line[(size_t) wp], l1, l2, h1, h2);
                d[n] = x * (1.0f - mix) + filtered * mix;
                line[(size_t) wp] = std::tanh (x + filtered * fb);
                wp = (wp + 1) % ds;
            }
        }
        return;
    }

    // Stereo ping-pong: echoes bounce L<->R via cross-feedback. Mid mode pans them 35%
    // (0.675 / 0.325) instead of the full 100% / 0% of classic ping-pong.
    const float width = (mode == 1) ? 1.0f : 0.35f;
    auto* dL = buf.getWritePointer (0);
    auto* dR = buf.getWritePointer (1);
    std::vector<float> lineL ((size_t) ds, 0.0f), lineR ((size_t) ds, 0.0f);
    int wp = 0;
    double l1L = 0, l2L = 0, h1L = 0, h2L = 0, l1R = 0, l2R = 0, h1R = 0, h2R = 0;
    for (int n = 0; n < buf.getNumSamples(); ++n)
    {
        const float inL = dL[n], inR = dR[n];
        const float monoIn = (inL + inR) * 0.5f;
        const float fL = (float) svf (lineL[(size_t) wp], l1L, l2L, h1L, h2L);
        const float fR = (float) svf (lineR[(size_t) wp], l1R, l2R, h1R, h2R);

        lineL[(size_t) wp] = std::tanh (monoIn + fR * fb);   // input enters L; R echo feeds back
        lineR[(size_t) wp] = std::tanh (fL * fb);            // L echo bounces to R

        const float wetL = fL * (0.5f + 0.5f * width) + fR * (0.5f - 0.5f * width);
        const float wetR = fR * (0.5f + 0.5f * width) + fL * (0.5f - 0.5f * width);
        dL[n] = inL * (1.0f - mix) + wetL * mix;
        dR[n] = inR * (1.0f - mix) + wetR * mix;
        wp = (wp + 1) % ds;
    }
}
} // namespace

void FxRack::process (juce::AudioBuffer<float>& buffer, double sampleRate, double tempoBpm) const
{
    for (const auto& s : slotArray)
    {
        if (! s.on || ! fxInfo (s.type).implemented)
            continue;
        switch (s.type)
        {
            case FxType::Filter:     applyFilter (buffer, sampleRate, s.params);            break;
            case FxType::Distortion: applyDistortion (buffer, s.params);                    break;
            case FxType::Bitcrush:   applyBitcrush (buffer, s.params);                      break;
            case FxType::Delay:      applyDelay (buffer, sampleRate, s.params, tempoBpm);   break;
            default: break;
        }
    }
}
}
