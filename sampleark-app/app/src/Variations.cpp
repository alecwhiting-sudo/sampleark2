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

    // Envelope / crossfade scopes -> prep fades.
    if (on (scope, Scope::Everything) || on (scope, Scope::Envelopes) || on (scope, Scope::Crossfades))
    {
        v.prep.fadeInMs     = juce::jmax (0.0, v.prep.fadeInMs     + bip() * pert * 40.0);
        v.prep.fadeOutMs    = juce::jmax (0.0, v.prep.fadeOutMs    + bip() * pert * 60.0);
        v.prep.outFadeOutMs = juce::jmax (0.0, v.prep.outFadeOutMs + bip() * pert * 200.0);
    }
}
}
