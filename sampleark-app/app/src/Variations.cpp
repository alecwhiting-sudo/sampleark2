#include "Variations.h"
#include <cmath>

namespace sa
{
const char* scopeName (Scope s)
{
    switch (s)
    {
        case Scope::Everything: return "Everything";
        case Scope::Plugins:    return "Plug-ins";
        case Scope::Filter:     return "Filter";
        case Scope::Envelopes:  return "Envelopes";
        case Scope::Dynamics:   return "Dynamics";
        case Scope::Crossfades: return "Crossfades";
        case Scope::Timing:     return "Timing";
        case Scope::Pitch:      return "Pitch";
        case Scope::Mangle:     return "Mangle";
        case Scope::Tail:       return "Tail";
        default:                return "?";
    }
}

namespace
{
bool on (const ScopeMask& m, Scope s) { return m[(size_t) s]; }

// Is (effect type, param) eligible to mutate under the chosen scopes?
bool paramInScope (FxType type, int pi, const ScopeMask& m)
{
    if (on (m, Scope::Everything) || on (m, Scope::Plugins)) return true;
    switch (type)
    {
        case FxType::Filter:      return on (m, Scope::Filter);
        case FxType::Compression: return on (m, Scope::Dynamics);
        case FxType::Limiter:     return on (m, Scope::Dynamics);
        case FxType::Distortion:  return on (m, Scope::Mangle);
        case FxType::Bitcrush:    return on (m, Scope::Mangle);
        case FxType::Reverb:      return on (m, Scope::Tail);
        case FxType::Delay:       return (pi == 0 || pi == 5) ? on (m, Scope::Timing) : on (m, Scope::Tail);
        default:                  return false;   // Autopan: only Everything/Plug-ins
    }
}
}

void mutate (Variation& v, const Variation& base, juce::uint32 seed, float level01, const ScopeMask& scope)
{
    v.prep  = base.prep;
    v.rack  = base.rack;
    v.trans = base.trans;
    v.seed  = seed;

    juce::Random rng ((juce::int64) seed);
    const float lvl  = juce::jlimit (0.0f, 1.0f, level01);
    const float pert = 0.06f + lvl * 0.60f;                  // max continuous-param swing
    auto bip = [&rng] { return rng.nextFloat() * 2.0f - 1.0f; };

    // Effect parameters.
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& slot = v.rack.slots()[(size_t) s];
        const auto& info = fxInfo (slot.type);
        if (! info.implemented) continue;

        for (int pi = 0; pi < (int) slot.params.size(); ++pi)
        {
            if (! paramInScope (slot.type, pi, scope)) continue;
            const bool seg = pi < (int) info.params.size() && info.params[pi].isSeg();
            if (seg)
            {
                // Discrete: occasionally jump to another option (more likely when wilder).
                const int opts = (int) info.params[pi].options.size();
                if (opts > 1 && rng.nextFloat() < lvl * 0.5f)
                    slot.params[(size_t) pi] = (float) rng.nextInt (opts);
            }
            else
            {
                slot.params[(size_t) pi] = juce::jlimit (0.0f, 1.0f, slot.params[(size_t) pi] + bip() * pert);
            }
        }

        // Bypass flips: from Massive up; reorder only when Unsafe.
        if ((on (scope, Scope::Everything) || on (scope, Scope::Plugins)) && lvl > 0.5f
            && rng.nextFloat() < (lvl - 0.5f) * 0.4f)
            slot.on = ! slot.on;
    }
    if (lvl > 0.8f && rng.nextFloat() < (lvl - 0.8f) * 1.5f)
        v.rack.move (rng.nextInt (kNumSlots), rng.nextInt (kNumSlots));

    // Transformer curves: reshape the ones that are ON. Severity controls the character —
    //   Gentle  : a coherent whole-curve drift (±5–10%), shape unchanged.
    //   Vibing+ : a smooth random "difference curve" pushes different parts of the curve by
    //             different amounts — ≈±5% (Vibing), ±10% (Massive), ±25% (Unsafe).
    // All clamped to 0..1. Each ON transformer gets its own independent randomness.
    {
        // per-point variation amplitude by severity zone (continuous between zone centres)
        const float varyAmt = lvl < 0.375f ? juce::jmap (lvl, 0.0f,   0.375f, 0.0f,  0.05f)
                            : lvl < 0.625f ? juce::jmap (lvl, 0.375f, 0.625f, 0.05f, 0.10f)
                            :                juce::jmap (lvl, 0.625f, 1.0f,   0.10f, 0.25f);

        for (auto& t : v.trans)
        {
            if (! t.on) continue;
            const int tslot = juce::jlimit (0, kNumSlots - 1, t.slot);
            const bool inScope = on (scope, Scope::Everything)
                || (t.kind == TransTarget::EffectParam
                        ? paramInScope (v.rack.slots()[(size_t) tslot].type, t.param, scope)
                        : on (scope, Scope::Plugins));   // pre/post-amp transformers
            if (! inScope) continue;
            if ((int) t.curve.size() != kCurvePoints) continue;

            // coherent whole-curve drift — strongest at Gentle, eased back as per-point grows
            const float gShift = (rng.nextBool() ? 1.0f : -1.0f)
                               * (0.05f + rng.nextFloat() * 0.05f) * (1.0f - 0.4f * lvl);

            // smooth random difference curve: a few random anchors interpolated across the curve
            constexpr int kAnchors = 8;
            float anchor[kAnchors + 1];
            for (int a = 0; a <= kAnchors; ++a) anchor[a] = bip() * varyAmt;

            for (int i = 0; i < kCurvePoints; ++i)
            {
                const float fp = (float) i / (float) (kCurvePoints - 1) * (float) kAnchors;
                const int   k  = juce::jmin (kAnchors - 1, (int) fp);
                const float d  = anchor[k] + (anchor[k + 1] - anchor[k]) * (fp - (float) k);
                t.curve[(size_t) i] = juce::jlimit (0.0f, 1.0f, t.curve[(size_t) i] + gShift + d);
            }
            t.shapeType = -1;   // now a custom (mutated) curve, not a clean preset
        }
    }

    // Envelope / crossfade scopes -> prep fades.
    if (on (scope, Scope::Everything) || on (scope, Scope::Envelopes) || on (scope, Scope::Crossfades))
    {
        v.prep.fadeInMs     = juce::jmax (0.0, v.prep.fadeInMs     + bip() * pert * 40.0);
        v.prep.fadeOutMs    = juce::jmax (0.0, v.prep.fadeOutMs    + bip() * pert * 60.0);
        v.prep.outFadeOutMs = juce::jmax (0.0, v.prep.outFadeOutMs + bip() * pert * 200.0);
    }
}
}
