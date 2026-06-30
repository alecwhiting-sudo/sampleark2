# Arch05 Mutation Spec

## Purpose

The canonical specification for SampleArk's mutation system: how the **Mutate** action turns one prepped sample into a batch of useful variations. This locks the model that Arch02 sketched and the UI in Arch04 exposes. Companion: `Arch06_UI_Requirements.md` maps the controls; this doc defines the engine behaviour behind them.

## The two-axis model

Mutation is **Depth × Affects**:

- **Depth** — *how hard* the machine pushes. One continuous mutation-severity value (0–100%).
- **Affects** — *what it is allowed to touch*. A set of parameter-category facets.

On **Mutate**, the engine generates a batch of candidate variations (default 20). For each candidate it re-samples every in-scope parameter to a new value drawn from the range its current depth permits, using a deterministic per-candidate seed. Out-of-scope parameters are left at their current values.

## Depth — one severity scale, four named zones

Depth is a single value, not four switches. Gentle / Vibing / Massive / Unsafe are **named zones along one 0–100% scale** (the DEPTH slider). The user drags for fine control or clicks a zone to snap to its midpoint.

| Zone | Range | Magnitude (per param) | Structural ops (bypass / reorder / type-switch) | Character |
|---|---|---|---|---|
| **Gentle** | 0–25% | ±0–10% of musical range | none | always usable, subtle |
| **Vibing** | 25–50% | ±10–30% | rare, only where musical | groove, still safe |
| **Massive** | 50–75% | ±30–70%, full musical range | yes | dramatic, renders clean |
| **Unsafe** | 75–100% | full 0–100% incl. danger zones | yes + random | self-osc, clipping, glitch, deliberate breakage |

"Musical range" = a per-parameter safe sub-range defined in metadata; "danger zone" = the remainder up to the absolute parameter limits.

## Affects — scope facets

Each built-in parameter is tagged with a `category`. The AFFECTS facets select parameters by category. `Everything` is the master.

| Facet | Category meaning | Parameters it touches |
|---|---|---|
| **Everything** | master — enables all facets below | all in-scope |
| **Filter** | spectral shaping | Filter cutoff / reso / type; tone controls on other FX |
| **Envelopes** | amplitude-over-time | Shape Attack / Hold / Release; compressor attack / release time-constants |
| **Dynamics** | level control | Compressor threshold / ratio; Limiter ceiling |
| **Crossfades** | boundary fades / click protection | Fade In / Out length + curve |
| **Timing** | time placement & sync | Trim start/end position, Delay time, Autopan rate |
| **Pitch** | tuning & pitch | Tune, Transpose, Formant, warp/resample mode |
| **Mangle** | destructive timbral character | Distortion drive, Bitcrush bits/rate, waveshaping |
| **Tail** | space & decay | Reverb size/decay/mix, Delay feedback/mix |
| **Plug-ins** | catch-all: remaining rack params; in v1.5, hosted AU/VST params | Mix amounts, misc rack params; (v1.5) plugin params |

### "Everything" exclusivity logic

`Everything` is mutually exclusive with the specific facets:
- When `Everything` is on, the others render as "included" (dim-orange) but are not individually selected.
- Selecting any specific facet turns `Everything` **off** and toggles that facet.
- Deselecting the last active facet falls back to `Everything`.

## Depth × Scope matrix (LOCKED)

Concrete behaviour per cell. Magnitudes are the deviation a candidate may take for a parameter in that scope at that depth.

| Scope | Gentle | Vibing | Massive | Unsafe |
|---|---|---|---|---|
| **Filter** | ±5% cutoff, tiny reso | ±20% cutoff, mild reso | wide sweep, type may switch | self-oscillating reso, extreme cutoff |
| **Envelopes** | ±5% A/H/R | ±20%, snappier/looser | punch↔pad reshapes | zero-attack clicks, infinite hold |
| **Dynamics** | ±5% thresh/ratio | ±20%, more pump | hard squash / heavy limit | over-compress, pumping/distortion |
| **Crossfades** | ±few ms | ±10–20 ms | long fades / near-gate | 0 ms (clicks) or fade eats sample |
| **Timing** | ±2 ms start nudge | ±10 ms, slight delay shift | start jumps, delay re-times | start past transient, runaway delay |
| **Pitch** | ±10 cents | ±1 semitone | ±5–7 semis, formant shift | octaves, alias, formant break |
| **Mangle** | barely-there grit | audible drive/crush | heavy saturation / 8-bit | full-destroy, fold, gated noise |
| **Tail** | ±5% size/decay | ±25%, more space | huge verb / long feedback | infinite feedback, runaway wash |
| **Plug-ins** | ±5% safe params | ±20% | structural toggles/reorder | random bypass + random order |

## Parameter metadata

Each built-in parameter carries a `ParameterMutationSpec` (per Arch02):

```text
ParameterMutationSpec
  parameterId
  displayName
  category            # which AFFECTS facet (filter, envelope, dynamics, ...)
  defaultValue
  musicalRange        # safe sub-range used by Gentle..Massive scaling
  absoluteRange       # full limits, reachable only at Unsafe
  scaleType           # linear | log | stepped
  allowOrderMutation  # may participate in chain reorder (Massive+)
  allowBypassMutation # may be randomly bypassed (Unsafe)
```

The engine maps the current depth (0–100%) to a fraction of `musicalRange` (Gentle..Massive) or into `absoluteRange` (Unsafe), per the matrix.

## Determinism

A variation stores its `mutationSeed`, `depth`, and active `scopes` alongside the trim/gain/rack snapshot, so it re-renders to identical audio every time (Arch03 invariant). **Mutate** re-rolls seeds for the batch; re-rendering an existing variation does not.

## UI surface

The MUTATE module (Arch04 / Arch06):
- **DEPTH** = the severity slider (named zones + % readout). This is the single severity value above.
- A **funnel** visually links the selected depth down into the AFFECTS chips (driver → targets).
- **AFFECTS** = the facet chips, with the exclusivity logic above.
- The **MUTATE** button generates the batch; its sublabel echoes `N candidates · <Zone> NN%`.

## Implementation status (2026-06-30)

The depth model is **built** (M5): `zoneMag()` maps the DEPTH value to per-param swing through the four-zone matrix; `musicalRange()` bounds Gentle..Massive to a safe sub-range and Unsafe expands toward the absolute limits (danger zone); `categoryScope()` tags each param to an AFFECTS facet; structural ops (bypass/reorder/type-switch) are gated to Massive+. Determinism (per-variation seed) holds. **The Limiter is excluded from mutation entirely** (a safety net) — an addition beyond this spec.

## Open / deferred

- **Blueprints** (save/recall a depth+affects+rack setup) were **built then removed** — storing mutation behaviour next to the AFFECTS chips was confusing. The remaining want is a **whole-sound preset** system (FX + trim + rack), kept *separate* from mutation. Candidate for Arch07.
- Per-effect engineering ranges (Hz, dB, ms) are defined per effect as each is built (M3/M5), within the `musicalRange`/`absoluteRange` model above.
- "Everything" exclusivity logic and the funnel UI are built; **zone click-to-snap** on the DEPTH slider is not (drag works).
</content>
