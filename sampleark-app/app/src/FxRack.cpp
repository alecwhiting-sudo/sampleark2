#include "FxRack.h"
#include <cmath>
#include <memory>

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

// ---------------- effect DSP: stateful block-processors ----------------
// Each effect carries its state across blocks, so params can vary per block (modulation)
// while filter/delay state stays continuous. Same per-sample math as before -> with static
// params the output matches the old whole-buffer code.
namespace
{
constexpr int kBlock = 64;

struct Proc
{
    virtual ~Proc() = default;
    virtual void prepare (double sr, double tempo, const std::vector<float>& base) = 0;
    virtual void process (juce::AudioBuffer<float>& buf, int start, int num, const std::vector<float>& p) = 0;
};

struct FilterProc : Proc
{
    double sr = 44100.0, aAtk = 0.0, aRel = 0.0, ic1[2] = {0,0}, ic2[2] = {0,0};
    float envf[2] = {0,0};
    void prepare (double s, double, const std::vector<float>&) override
    {
        sr = s; ic1[0]=ic1[1]=ic2[0]=ic2[1]=0.0; envf[0]=envf[1]=0.0f;
        aAtk = 1.0 - std::exp (-1.0 / (0.005 * sr));
        aRel = 1.0 - std::exp (-1.0 / (0.080 * sr));
    }
    void process (juce::AudioBuffer<float>& buf, int start, int num, const std::vector<float>& p) override
    {
        const double cut = juce::jlimit (0.0f, 1.0f, p[0]);
        const double res = juce::jlimit (0.0f, 1.0f, p[1]);
        const float  env = juce::jlimit (0.0f, 1.0f, p.size() > 2 ? p[2] : 0.0f);
        const int type = (int) std::round (p.size() > 3 ? p[3] : 0.0f);
        const double baseFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (1000.0, cut));
        const double k = 1.0 / (0.5 + res * 9.5);
        for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            double i1 = ic1[ch], i2 = ic2[ch]; float E = envf[ch];
            for (int n = start; n < start + num; ++n)
            {
                const double x = d[n];
                const float rect = std::abs ((float) x);
                E += (rect > E ? aAtk : aRel) * (rect - E);
                double fc = juce::jlimit (20.0, sr * 0.45, baseFc * std::pow (2.0, (double) (env * 4.0f * E)));
                const double g = std::tan (juce::MathConstants<double>::pi * fc / sr);
                const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
                const double v3 = x - i2, v1 = a1 * i1 + a2 * v3, v2 = i2 + a2 * i1 + a3 * v3;
                i1 = 2.0 * v1 - i1; i2 = 2.0 * v2 - i2;
                d[n] = (float) ((type == 0) ? v2 : (type == 2 ? (x - k * v1 - v2) : v1));
            }
            ic1[ch] = i1; ic2[ch] = i2; envf[ch] = E;
        }
    }
};

struct DistortionProc : Proc
{
    float y[2] = {0,0};
    void prepare (double, double, const std::vector<float>&) override { y[0]=y[1]=0.0f; }
    void process (juce::AudioBuffer<float>& buf, int start, int num, const std::vector<float>& p) override
    {
        const float drive = juce::jlimit (0.0f, 1.0f, p[0]);
        const float tone  = juce::jlimit (0.0f, 1.0f, p[1]);
        const float mix   = juce::jlimit (0.0f, 1.0f, p[2]);
        const double k = 1.0 + drive * 24.0, nk = std::tanh (k);
        const float a = 0.05f + tone * 0.95f;
        for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            float Y = y[ch];
            for (int n = start; n < start + num; ++n)
            {
                const float x = d[n];
                const float wet = (float) (std::tanh (x * k) / nk);
                Y += a * (wet - Y);
                d[n] = x * (1.0f - mix) + Y * mix;
            }
            y[ch] = Y;
        }
    }
};

struct BitcrushProc : Proc
{
    float held[2] = {0,0}; long long cnt[2] = {0,0};
    void prepare (double, double, const std::vector<float>&) override { held[0]=held[1]=0.0f; cnt[0]=cnt[1]=0; }
    void process (juce::AudioBuffer<float>& buf, int start, int num, const std::vector<float>& p) override
    {
        const float bits = juce::jlimit (0.0f, 1.0f, p[0]);
        const float rate = juce::jlimit (0.0f, 1.0f, p[1]);
        const float mix  = juce::jlimit (0.0f, 1.0f, p[2]);
        const double levels = std::pow (2.0, 1.0 + bits * 15.0);
        const int hold = 1 + (int) std::round ((1.0f - rate) * 31.0f);
        for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            float H = held[ch]; long long C = cnt[ch];
            for (int n = start; n < start + num; ++n)
            {
                const float x = d[n];
                if (C % hold == 0)
                    H = (float) (std::round ((x * 0.5 + 0.5) * (levels - 1.0)) / (levels - 1.0) * 2.0 - 1.0);
                ++C;
                d[n] = x * (1.0f - mix) + H * mix;
            }
            held[ch] = H; cnt[ch] = C;
        }
    }
};

struct DelayProc : Proc
{
    double sr = 44100.0;
    int ds = 1, wpL = 0, wpR = 0;
    std::vector<float> lineL, lineR;
    double l1L=0,l2L=0,h1L=0,h2L=0, l1R=0,l2R=0,h1R=0,h2R=0;
    void prepare (double s, double tempo, const std::vector<float>& base) override
    {
        sr = s;
        ds = juce::jmax (1, (int) (sa::delayTimeSeconds (base, tempo) * sr));   // time not modulated
        lineL.assign ((size_t) ds, 0.0f); lineR.assign ((size_t) ds, 0.0f);
        wpL = wpR = 0; l1L=l2L=h1L=h2L=l1R=l2R=h1R=h2R=0.0;
    }
    void process (juce::AudioBuffer<float>& buf, int start, int num, const std::vector<float>& p) override
    {
        const float fb  = juce::jlimit (0.0f, 1.0f, p[1]) * 0.95f;
        const float lp  = juce::jlimit (0.0f, 1.0f, p.size() > 2 ? p[2] : 1.0f);
        const float hp  = juce::jlimit (0.0f, 1.0f, p.size() > 3 ? p[3] : 0.0f);
        const float mix = juce::jlimit (0.0f, 1.0f, p.size() > 4 ? p[4] : 0.3f);
        const int mode = (int) std::round (p.size() > 6 ? p[6] : 0.0f);
        const double pi = juce::MathConstants<double>::pi;
        const double lpFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (1000.0, (double) lp));
        const double hpFc = juce::jlimit (20.0, sr * 0.45, 20.0 * std::pow (500.0,  (double) hp));
        const double kQ = 1.0 / 1.3;
        const double gL = std::tan (pi * lpFc / sr), a1L = 1.0/(1.0+gL*(gL+kQ)), a2L = gL*a1L, a3L = gL*a2L;
        const double gH = std::tan (pi * hpFc / sr), a1H = 1.0/(1.0+gH*(gH+kQ)), a2H = gH*a1H, a3H = gH*a2H;
        const bool lpA = (lp < 0.999f), hpA = (hp > 0.001f);
        auto svf = [&] (double s, double& l1, double& l2, double& h1, double& h2)
        {
            if (lpA) { const double v3=s-l2, v1=a1L*l1+a2L*v3, v2=l2+a2L*l1+a3L*v3; l1=2*v1-l1; l2=2*v2-l2; s=v2; }
            if (hpA) { const double w3=s-h2, w1=a1H*h1+a2H*w3, w2=h2+a2H*h1+a3H*w3; h1=2*w1-h1; h2=2*w2-h2; s=s-kQ*w1-w2; }
            return s;
        };

        if (mode == 0 || buf.getNumChannels() < 2)
        {
            for (int ch = 0; ch < buf.getNumChannels() && ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                auto& line = (ch == 0) ? lineL : lineR;
                int& wp = (ch == 0) ? wpL : wpR;
                double& L1 = (ch==0)?l1L:l1R; double& L2 = (ch==0)?l2L:l2R;
                double& H1 = (ch==0)?h1L:h1R; double& H2 = (ch==0)?h2L:h2R;
                for (int n = start; n < start + num; ++n)
                {
                    const float x = d[n];
                    const float f = (float) svf (line[(size_t) wp], L1, L2, H1, H2);
                    d[n] = x * (1.0f - mix) + f * mix;
                    line[(size_t) wp] = std::tanh (x + f * fb);
                    wp = (wp + 1) % ds;
                }
            }
            return;
        }

        const float width = (mode == 1) ? 1.0f : 0.35f;
        auto* dL = buf.getWritePointer (0);
        auto* dR = buf.getWritePointer (1);
        for (int n = start; n < start + num; ++n)
        {
            const float inL = dL[n], inR = dR[n], monoIn = (inL + inR) * 0.5f;
            const float fL = (float) svf (lineL[(size_t) wpL], l1L, l2L, h1L, h2L);
            const float fR = (float) svf (lineR[(size_t) wpL], l1R, l2R, h1R, h2R);
            lineL[(size_t) wpL] = std::tanh (monoIn + fR * fb);
            lineR[(size_t) wpL] = std::tanh (fL * fb);
            dL[n] = inL * (1.0f - mix) + (fL * (0.5f + 0.5f * width) + fR * (0.5f - 0.5f * width)) * mix;
            dR[n] = inR * (1.0f - mix) + (fR * (0.5f + 0.5f * width) + fL * (0.5f - 0.5f * width)) * mix;
            wpL = (wpL + 1) % ds;
        }
    }
};

std::unique_ptr<Proc> makeProc (sa::FxType t)
{
    switch (t)
    {
        case sa::FxType::Filter:     return std::make_unique<FilterProc>();
        case sa::FxType::Distortion: return std::make_unique<DistortionProc>();
        case sa::FxType::Bitcrush:   return std::make_unique<BitcrushProc>();
        case sa::FxType::Delay:      return std::make_unique<DelayProc>();
        default: return nullptr;
    }
}
} // namespace

void FxRack::process (juce::AudioBuffer<float>& buffer, double sampleRate, double tempoBpm,
                      const FxModulation& mod) const
{
    // Build the active chain (in order) with fresh, prepared processors.
    struct Entry { int slot; std::unique_ptr<Proc> proc; };
    std::vector<Entry> chain;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto& s = slotArray[i];
        if (! s.on || ! fxInfo (s.type).implemented) continue;
        if (auto pr = makeProc (s.type)) { pr->prepare (sampleRate, tempoBpm, s.params); chain.push_back ({ i, std::move (pr) }); }
    }

    const int N = buffer.getNumSamples();
    for (int start = 0; start < N; start += kBlock)
    {
        const int num = juce::jmin (kBlock, N - start);

        if (mod.preGain) { const float gp = mod.preGain (start); if (gp != 1.0f) buffer.applyGain (start, num, gp); }

        for (auto& e : chain)
        {
            const auto& info = fxInfo (slotArray[e.slot].type);
            std::vector<float> ep = slotArray[e.slot].params;     // effective (modulated) params for this block
            for (int pi = 0; pi < (int) ep.size(); ++pi)
            {
                if (pi < (int) info.params.size() && info.params[pi].isSeg()) continue;   // discrete: leave as-is
                const float add = mod.paramAdd ? mod.paramAdd (e.slot, pi, start) : 0.0f;
                ep[pi] = juce::jlimit (0.0f, 1.0f, ep[pi] + add);
            }
            e.proc->process (buffer, start, num, ep);
        }

        if (mod.postGain) { const float gp = mod.postGain (start); if (gp != 1.0f) buffer.applyGain (start, num, gp); }
    }
}
}
