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

const char* depthZoneName (float lvl)
{
    return lvl < 0.25f ? "Gentle" : lvl < 0.5f ? "Vibing" : lvl < 0.75f ? "Massive" : "Unsafe";
}

namespace
{
bool on (const ScopeMask& m, Scope s) { return m[(size_t) s]; }

// The AFFECTS facet a given (effect, param) belongs to (Arch05 category table). This is finer
// than per-effect: e.g. a distortion's tone is Filter, its mix is Plug-ins.
Scope categoryScope (FxType type, int pi)
{
    switch (type)
    {
        case FxType::Distortion:  return pi == 0 ? Scope::Mangle : pi == 1 ? Scope::Filter : Scope::Plugins;
        case FxType::Bitcrush:    return pi <= 1 ? Scope::Mangle : Scope::Plugins;
        case FxType::Compression: return (pi == 2 || pi == 3) ? Scope::Envelopes : Scope::Dynamics;   // atk/rel = envelope
        case FxType::Filter:      return Scope::Filter;
        case FxType::Delay:       return (pi == 0 || pi == 5) ? Scope::Timing
                                       : (pi == 2 || pi == 3) ? Scope::Filter            // LP/HP tone
                                       : pi == 6              ? Scope::Plugins           // mode
                                                             : Scope::Tail;             // feedback/mix
        case FxType::Reverb:      return Scope::Tail;
        case FxType::Limiter:     return pi == 1 ? Scope::Envelopes : Scope::Dynamics;   // release = envelope
        case FxType::Autopan:     return pi == 0 ? Scope::Timing : Scope::Plugins;
        default:                  return Scope::Plugins;
    }
}

// Is (effect, param) eligible to mutate under the chosen scopes?
bool paramInScope (FxType type, int pi, const ScopeMask& m)
{
    if (on (m, Scope::Everything) || on (m, Scope::Plugins)) return true;
    return on (m, categoryScope (type, pi));
}

struct Range { float lo, hi; };

// Per-parameter MUSICAL range (the safe sub-range Gentle..Massive stay within). At Unsafe the
// engine expands toward the absolute 0..1 limits ("danger zone"). Default = full range; only the
// parameters with a genuine danger zone are narrowed (Arch05 metadata, normalised units).
Range musicalRange (FxType type, int pi)
{
    switch (type)
    {
        case FxType::Filter:      if (pi == 1) return { 0.0f, 0.6f };  break;   // reso > 0.6 self-oscillates
        case FxType::Distortion:  if (pi == 0) return { 0.0f, 0.7f };  break;   // drive past 0.7 = full destroy
        case FxType::Bitcrush:    if (pi <= 1) return { 0.25f, 1.0f }; break;   // very low bits/rate = gated noise
        case FxType::Delay:       if (pi == 1) return { 0.0f, 0.6f };  break;   // feedback past 0.6 = runaway
        case FxType::Reverb:      if (pi == 1) return { 0.0f, 0.75f }; break;   // decay past 0.75 = endless wash
        case FxType::Compression: if (pi == 0) return { 0.2f, 1.0f };           // very low thresh = over-compress
                                  if (pi == 1) return { 0.0f, 0.8f };  break;   // ratio
        case FxType::Limiter:     if (pi == 0) return { 0.4f, 1.0f };  break;   // very low ceiling = brick-wall
        default: break;
    }
    return { 0.0f, 1.0f };
}

// Per-param swing magnitude (fraction of the available range) by depth, through the Arch05 matrix
// anchors: 0/0.10/0.30/0.70/1.0 at zone edges 0/0.25/0.5/0.75/1.0.
float zoneMag (float lvl)
{
    return lvl < 0.25f ? juce::jmap (lvl, 0.0f,  0.25f, 0.0f,  0.10f)
         : lvl < 0.50f ? juce::jmap (lvl, 0.25f, 0.50f, 0.10f, 0.30f)
         : lvl < 0.75f ? juce::jmap (lvl, 0.50f, 0.75f, 0.30f, 0.70f)
         :               juce::jmap (lvl, 0.75f, 1.00f, 0.70f, 1.00f);
}
}

void mutate (Variation& v, const Variation& base, juce::uint32 seed, float level01, const ScopeMask& scope)
{
    v.prep  = base.prep;
    v.rack  = base.rack;
    v.trans = base.trans;
    v.seed  = seed;

    juce::Random rng ((juce::int64) seed);
    const float lvl    = juce::jlimit (0.0f, 1.0f, level01);
    const float mag    = zoneMag (lvl);                       // swing as a fraction of the available range
    const float danger = lvl <= 0.75f ? 0.0f : (lvl - 0.75f) / 0.25f;   // Unsafe: 0..1 into the absolute range
    auto bip = [&rng] { return rng.nextFloat() * 2.0f - 1.0f; };

    // Effect parameters: each continuous param is re-rolled within the range its depth permits —
    // the musical sub-range for Gentle..Massive, expanding toward the absolute limits at Unsafe.
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
                // Discrete type-switch is structural: rare at Vibing, likely Massive+ (never Gentle).
                const int opts = (int) info.params[pi].options.size();
                if (opts > 1 && lvl > 0.25f && rng.nextFloat() < (lvl - 0.25f) * 0.6f)
                    slot.params[(size_t) pi] = (float) rng.nextInt (opts);
            }
            else
            {
                const Range mr = musicalRange (slot.type, pi);
                const float base = slot.params[(size_t) pi];
                float lo = mr.lo * (1.0f - danger);                    // expand toward 0..1 at Unsafe
                float hi = mr.hi + (1.0f - mr.hi) * danger;
                lo = juce::jmin (lo, base); hi = juce::jmax (hi, base); // never clamp away the user's base
                const float span = juce::jmax (1.0e-4f, hi - lo);
                slot.params[(size_t) pi] = juce::jlimit (lo, hi, base + bip() * mag * span);
            }
        }

        // Structural ops are gated to Massive+ (none at Gentle/Vibing), more likely toward Unsafe.
        const bool structuralScope = on (scope, Scope::Everything) || on (scope, Scope::Plugins);
        if (structuralScope && lvl > 0.5f && rng.nextFloat() < (lvl - 0.5f) * 0.5f)
            slot.on = ! slot.on;                                       // random bypass
    }
    if ((on (scope, Scope::Everything) || on (scope, Scope::Plugins)) && lvl > 0.5f
        && rng.nextFloat() < (lvl - 0.5f) * 0.6f)
        v.rack.move (rng.nextInt (kNumSlots), rng.nextInt (kNumSlots));   // random reorder, Massive+

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

    // Crossfade scope -> source fades; Envelope scope -> output fade tail.
    if (on (scope, Scope::Everything) || on (scope, Scope::Crossfades))
    {
        v.prep.fadeInMs  = juce::jmax (0.0, v.prep.fadeInMs  + bip() * mag * 40.0);
        v.prep.fadeOutMs = juce::jmax (0.0, v.prep.fadeOutMs + bip() * mag * 60.0);
    }
    if (on (scope, Scope::Everything) || on (scope, Scope::Envelopes))
        v.prep.outFadeOutMs = juce::jmax (0.0, v.prep.outFadeOutMs + bip() * mag * 200.0);
}
}
