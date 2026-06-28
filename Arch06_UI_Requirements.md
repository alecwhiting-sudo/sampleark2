# Arch06 UI Requirements

## Purpose

Maps every control on the main screen to a technical requirement: what it does, its range/units, its effect on the signal chain, its mutation category, and which milestone builds it. Companion docs: `Arch04_Design_Spec.md` (visual/interaction), `Arch05_Mutation_Spec.md` (mutation engine), `Arch03_Implementation_Plan.md` (milestones).

**Legend** — Milestone: M0 static · M1 audible spine · M2 prep+export · M3 rack v1 · M4 variations · M5 full rack+mutation · M6 tempo/warp. Mutation category per Arch05 (— = not mutated).

## Signal chain (reference)

`Source → Trim region → Fades/click protection → Pitch/Tempo → Rack (8 slots) → Output gain / safety limiter → Preview or Offline render`

Invariants (Arch03): preview matches print; deterministic per seed; non-destructive until Write; ≥32-bit float internal.

## Toolbar

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| PLAY | Plays the **prepped source one-shot** (trim + shape applied) through the audio device | — | — | M1 |
| STOP | Halts all playback (source + any auditioning variation) | — | — | M1 |
| LOAD SAMPLE | Click → file browser for `.wav`/`.aif`; **also** accepts a Finder drag-drop onto the SAMPLE panel / window. Decodes to float, becomes source, source file untouched | wav/aiff | — | M1 |
| Filename chip | Displays loaded file name + length + rate/depth | display | — | M1 |
| TEMPO − / value / + | Session tempo; steppers ±1 | 40–220 BPM, default 120 | Timing (synced FX) | M6 |
| DETECT | Auto-detect tempo from loop material; result editable | — | — | M6 |
| ⚙ Preferences | Opens output defaults + app settings (see Write/Export) | — | — | M2 |

## SAMPLE panel

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| Waveform | Drawn from decoded audio; primary drop target | — | — | M1 |
| Trim Start / End handles | Draggable region bounds; linked to Trim knobs | 0..len (ms/frames) | Timing | M2 |
| Transient tick | Marks detected onset just after start | — | — | M2 |
| Playhead | Playback position | — | — | M1 |
| File facts | `rate / depth · frames · transient-locked` | display | — | M1 |
| "transient-locked" | Status: start marker snapped to a detected transient (vs free) | flag | — | M2 |

## PREP — Trim mode

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| Start | Region start position | 0..len | Timing | M2 |
| End | Region end position | 0..len | Timing | M2 |
| Transient | Pre-roll before detected transient: 0 = start on transient, higher = leave attack/air before it | 0–50 ms | Timing | M2 |
| Fade In | Boundary fade-in (click protection) | 0–250 ms, equal-power curve | Crossfades | M2 |
| Fade Out | Boundary fade-out | 0–250 ms | Crossfades | M2 |
| Auto-Detect | Snaps Start to detected transient | toggle | — | M2 |

## PREP — Shape mode

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| Gain | Output gain on the one-shot | −24…+24 dB (0 = unity) | — | M2 |
| Attack | Amplitude-envelope attack | 0–100 ms | Envelopes | M2 |
| Hold | Envelope hold | 0–500 ms | Envelopes | M2 |
| Release | Envelope release to silence | 0–2000 ms | Envelopes | M2 |
| Normalize | Peak-normalize at print | toggle, target −1.0 dBTP | — | M2 |

## PREP — Pitch mode

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| Tune | Fine pitch | ±100 cents | Pitch | M6 |
| Transpose | Coarse pitch | ±24 semitones | Pitch | M6 |
| Formant | Formant shift (Warp mode only) | ±12 | Pitch | M6 |
| Time: Warp / Resample | Warp = pitch-preserving (deferred engine) · Resample = speed+pitch linked (old-school) | 2-way | Pitch | M6 |

## FX RACK

| Control | Behaviour | Range / units | Mutation cat. | Milestone |
|---|---|---|---|---|
| Slot row (×8) | Built-in processor; click to select into editor | — | — | M3 |
| Power LED | Toggle bypass | on/off | Plug-ins (Unsafe random) | M3 |
| Drag handle | Reorder chain (signal flows top→bottom) | order 1–8 | Plug-ins (Massive+) | M3 |
| CORE badge | Built-in indicator (VST in v1.5) | display | — | M3 |
| Swap ⇄ | Replace with hosted AU/VST — **inert/"v1.5" in v1** | — | — | v1.5 |

Default order: Distortion · Bitcrush · Compression · Delay · Reverb · Filter · Limiter · Autopan. Compression + Autopan default off. (Per-effect parameter ranges defined as each effect is built.)

## EFFECT EDITOR (selected slot)

Visual graph + knobs per Arch04. Filter/Distortion/Bitcrush/Delay = M3; Compression/Reverb/Limiter/Autopan = M5. Each effect's params carry mutation categories per Arch05 (Filter→Filter, Distortion/Bitcrush→Mangle, Delay/Reverb→Tail, Compression/Limiter→Dynamics, Autopan→Timing).

## MUTATE module

| Control | Behaviour | Range / units | Milestone |
|---|---|---|---|
| DEPTH slider | Single mutation-severity value; Gentle/Vibing/Massive/Unsafe zones; drag fine or click-snap | 0–100% | M4 |
| Funnel | Visual: selected depth fans into the affects targets | — | M0/M4 |
| AFFECTS chips | Select parameter facets to mutate (10 facets incl. Dynamics); "Everything" exclusivity logic | per Arch05 | M4 |
| MUTATE button | Generate the candidate batch; sublabel `N candidates · <Zone> NN%` | — | M4 |

Behaviour governed by `Arch05_Mutation_Spec.md`.

## VARIATIONS

| Control | Behaviour | Range / units | Milestone |
|---|---|---|---|
| Candidate rows | 20 generated; mini-waveform, name `<source>_vNN` | 20 | M4 |
| ▶ per row | Audition that candidate | — | M4 |
| Select / fav dot | Toggle selection (default first 8) | — | M4 |
| M (mute) | Mute + deselect a candidate | — | M4 |
| SELECTED N / 8 | Count; green at exactly 8 | — | M4 |
| Audition playlist | Plays through selected like a playlist, default 1 s gap between samples | 1 s default | M4 |

Generate count (20) and default-selected (8) configurable later; trial limiter counts **written** outputs in multiples of 8.

## WRITE / EXPORT

**Model:** technical output settings are **global defaults set once**; per-write only asks Name + Folder (both with smart suggestions).

| Item | Where | Behaviour | Default |
|---|---|---|---|
| Bit depth | Preferences (global) | 16 / 24 / 32f; dither auto on fixed-point (never on 32f, never double) | 24-bit |
| Sample rate | Preferences (global) | source / 44.1k / 48k; best-quality SRC if converting | source |
| Dither | Preferences (global) | auto / off | auto |
| Normalize | Preferences (global) | default on/off + target | per Shape / off |
| **Name** | Per-write prompt | Always shown with an intelligent suggestion from source stem + variation index (e.g. `kick_raw_07_v01`) | smart |
| **Folder** | Per-write prompt | Intelligent suggestion incl. **last-used folder**; remembered between sessions | last-used |
| WRITE SELECTED → | Footer | Renders selected candidates offline at the global format to the chosen folder | — |
| KEEP PLAYING | Footer | **Cut** — Write is non-destructive, so "keep exploring" needs no button (pending final veto) | removed |

Export quality rules per Arch02 (tail-aware render, no clicks, consistent gain, dither only when reducing to fixed point). M2 builds single/unchanged export; M4 builds batch write of selected.

## Still to define (as each milestone lands)

- Per-effect engineering ranges (Hz / dB / ms / sync) — defined when each effect is built (M3/M5).
- Tempo-detection algorithm + quality gate (M6).
- Trial-limiter UI surface (where remaining allowance shows) — when licensing work starts.
- Blueprints UI (deferred post-M4).
</content>
