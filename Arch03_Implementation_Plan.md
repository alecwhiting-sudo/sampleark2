# Arch03 Implementation Plan

## Purpose

This is the working implementation tracker for the SampleArk working-title product.

Arch01 defines the human/product intent. Arch02 defines the technical architecture. Arch03 tracks what we are building and what has been completed.

## Status Key

- `[ ]` Not started.
- `[~]` In progress.
- `[x]` Complete.
- `[!]` Blocked or needs decision.

## Current Build Status (2026-06-30)

**M0–M5 are complete and pushed.** The app builds and runs (`sampleark-app/`, JUCE 8.0.9, CMake). The full core loop works end to end: browse/drop a sample → see + hear it → prep + rack + transformers → mutate (or hand-capture) a batch of variations → recall/audition/playlist → favourite → write to disk.

| Milestone | Status |
|---|---|
| **M0** Skeleton on screen | ✅ Complete |
| **M1** Audible spine | ✅ Complete |
| **M2** One-shot prep | ✅ Complete (export 16/24/32-bit; see note) |
| **M3** Built-in rack v1 (Filter/Distortion/Bitcrush/Delay) | ✅ Complete |
| **M3a** Transformers & flexible layout | ✅ Complete (2 refinements deferred) |
| **M3b** Inputs browser | ✅ Complete (2 refinements deferred) |
| **M4** Variations — the magic moment | ✅ Complete |
| **M5** Full rack + mutation depth | ✅ Complete (Blueprints built then removed) |
| **M6** Tempo & warp | ⬜ Not started — the last v1 milestone |

Beyond the milestone list, several enhancements were added on request — dynamics meters, live modulation feedback, manual variation capture, an audio-interface selector, an app icon, and more (see **Post-plan enhancements** below). The remaining open items are collected in **What's left** at the end.

> The earlier "Ground Truth (2026-06-27)" note below ("nothing built yet") is **superseded** — kept only for history. Disregard it; this section is current.

## Ground Truth (2026-06-27) — superseded, historical

**(At the time of writing, nothing had been built.)** The repository contained only the Arch docs, UI mockups, and reference material.

A previous planning pass (in another tool) left this tracker full of `[x]` items and detailed "implementation notes" describing a `sampleark-app/` tree that did not exist. Those false claims were removed and the project restarted clean. (The tree described now genuinely exists — built milestone by milestone since.)

## Replanning Principle: Vertical Slices, Not Horizontal Layers

The previous plan built the engine horizontally — trim, then a full rack, then variations, then mutation depth, then tempo — almost all of it headless, with the visible/audible product (a window, a waveform, playback, a folder write) deferred to a perpetually-blocked "Phase 0". That is exactly backwards for this product. SampleArk succeeds or fails on one human loop:

> **See** a sample → **hear** it → **change** it → generate variations → **choose** favourites → **write** them to a folder.

So the work is sequenced as **milestones**, where each milestone is a build you can launch and judge. The engine grows only as far as the current milestone needs it. We do not build an effect we cannot yet hear, or a mutation system we cannot yet audition.

Every milestone must answer these five questions before it counts as complete:

- What can the user **see**?
- What can the user **hear**?
- What can the user **change**?
- What can the user **export**?
- What **decision** does this build help us make?

Design is judged against the shared PNG mockups in `UI mock ups input only`. Those PNGs are the primary visual/layout reference. Do not build or polish a web/HTML mockup as a UI target — the native app is the product.

### What this means in practice

- **Audible playback is the spine.** Until a real loaded sample draws a real waveform and plays through the audio device (Milestone 1), nothing else is worth building. Effects, variations, and mutation are meaningless if we cannot hear them.
- **Build the real layout shell early, but keep it thin.** Get the mockup layout on screen quickly, then wire it to real data. Do not perfect pixels before the panels show live state.
- **Two-part codebase from day one:** a dependency-light, headless-testable DSP/model **core**, and a JUCE **app shell** that drives it. This was the one genuinely good idea in the prior plan and it is worth keeping — it lets us unit-test audio logic without a UI. But the core is built *in service of* a runnable slice, never as an end in itself.
- **Ruthless deferral.** The four biggest time/risk sinks — AU/VST3 hosting, advanced serial/parallel rack containers, modern pitch-preserving warp, and AI — are **not** needed to prove the core promise. They are explicitly post-v1 and live at the bottom of this document.

## Invariants (hold these from the first commit)

These are cheap to keep continuously and expensive to retrofit:

- **Determinism.** A given variation state renders identical audio every time (fixed seeds, no wall-clock or uninitialised state in the signal path).
- **Preview / print parity.** What the user auditions must match what gets written to disk. Preview may be lower-latency, but it must not be a different algorithm.
- **Non-destructive until write.** Source files are never modified. All edits, effects, and mutations are state on top of an untouched decode.
- **High internal precision.** Decode to ≥32-bit float internally; 64-bit float at summing points. Dither only when reducing to fixed-point output, and never double-dither.
- **A central style system.** Colours, spacing, type, control sizes, waveform/knob styles live in one place. The app is dense; ad-hoc styling will make it feel messy fast.
- **One set of screens.** There is no Simple/Complex view mode and no global view toggle. The same layout serves everyone; complexity is revealed by folding/expanding in place (rack containers, effect detail, parameter panels), never by switching modes. Build Mac and Windows from the same codebase and the same screens.

## Design & Interaction Reference

**The authoritative UI spec is `Arch04_Design_Spec.md`** (repo root, alongside this file). It is self-contained: window/layout, design tokens, every zone, all 8 per-effect editor graphs, the full interaction model (exact knob-drag math, reorder, filter-graph drag), and first-run default state — all written to *our* decisions (no Simple/Complex, Mutate/Write, inert VST). Build M0/M3/M4 against Arch04.

The raw source it was distilled from is `design_handoff_sampleark/` (a Claude-design handoff): the `SampleArk v2.dc.html` prototype (open in a browser to see the screens working together — do not port its `support.js` runtime), its `README.md`, and the PNG stills in `UI mock ups input only`. These are visual reference only, not a shipping target and not an engine reference (their audio is mock data). Where the raw handoff disagrees with Arch04 (it predates our decisions on the view toggle, action names, and VST), **Arch04 wins.**

## Milestone Ladder

Each milestone produces a build that is easy to launch and shows a clear slice of the final workflow.

### M0 — Skeleton On Screen *(see)* — ✅ COMPLETE

Goal: the app opens and shows the real layout, even if every panel is static.

- [x] Decide app name / package id and folder layout (see Immediate Decisions).
- [x] Scaffold a CMake project with a headless `core` library and a JUCE `app` target.
- [x] Vendor JUCE (`sampleark-app/vendor/JUCE`, tag 8.0.9, git-ignored) and confirm a clean configure + build. *(Built/validated on Mac; Windows not yet validated — see What's left.)*
- [x] Open a native desktop window.
- [x] Lay out the zones from the design handoff: transport/file/tempo bar, source/waveform area, prep panel, FX rack list, selected-effect editor, variations browser, bottom mutate/write action strip.
- [x] Implement the dark style system from the handoff design tokens (palette, type, spacing, radii, control primitives — knob, LED, panel, well). `Theme.h`.

Decision this unlocks: does the overall shape, density, and hierarchy feel right before we wire anything?

### M1 — Audible Spine *(see + hear)* — ✅ COMPLETE

Goal: a real sample, drawn as a real waveform, that actually plays. Nothing else matters until this works.

- [x] Drag/drop and file-open import of WAV / AIFF.
- [x] Decode to internal float buffer; capture sample rate, channels, bit depth, frame count.
- [x] Draw the real waveform from decoded audio.
- [x] Set up the audio device and play/stop the source through it.
- [x] Show live file facts in the source header (rate / depth / frames).
- [x] Export the unchanged sample to disk (round-trip sanity check).

Decision this unlocks: is the native stack (JUCE audio device + custom drawing) solid enough to build the whole product on?

### M2 — One-Shot Prep *(change + export)* — ✅ COMPLETE

Goal: turn a raw sample into a clean, playable one-shot, and write it out at pro quality.

- [x] Draggable start/end trim markers on the waveform.
- [~] Transient / start auto-detect for percussive material. *(Auto-Detect control present; detection is basic — refine when M6 tempo-detect lands.)*
- [x] Fade in/out and click protection at boundaries (source fades + raised-cosine output fade).
- [x] Gain and normalize (Shape mode controls).
- [x] One-shot trigger playback of the prepped region.
- [x] Offline render of the prepped one-shot (background worker, tail-aware).
- [x] Export prepped one-shot to WAV (Cmd+E, 24-bit). *(16/32-bit + dither paths designed in but the export currently writes 24-bit; surface the bit-depth choice — see What's left.)*

Decision this unlocks: can a percussive sample become playable in under ~10 seconds, click-free, with trustworthy exports?

### M3 — Built-In Rack v1 *(change)* — ✅ COMPLETE

Goal: a focused, reorderable rack with a few strong, hearable effects and real visual editors.

- [x] Abstract rack-slot model: 8 top-level slots, each holding one built-in processor by default.
- [x] Bypass per slot; drag-to-reorder.
- [x] Implement the first effects with real DSP: Filter, Distortion, Bitcrush, Delay.
- [x] Visual editors for those four (per-effect response graphs, draggable) per the mockups.
- [x] Parameter model + state snapshots that round-trip to identical rendered output.

Decision this unlocks: does processing feel musical and immediate, and is the rack interaction (select / bypass / reorder) right?

### M3a — Transformers & Flexible Layout *(modulate + arrange)* — ✅ COMPLETE (refinements deferred)

> **Inserted phase.** Slots between M3 (FX rack — done) and M4 (variations). A deliberate foundation pass landed *before* the variations system so later complexity sits on a flexible, modulatable base. Lettered (`M3a`) to mark it as an out-of-band insertion; M4–M6 keep their numbers.

Goal: parameters can move *while the sample plays* (filter sweeps, bit-depth dives, pan motion…) via up to **8 editable modulation curves ("transformers")**, and the user can **show/hide the major panels** so the screen maximises around what they're working on.

**Part A — Flexible layout** (do first; the transformer window needs a home):
- [x] Top-bar toggle buttons to show/hide the major zones: SAMPLE, TRANS, FX, MUTATE, VARIATIONS.
- [x] Layout manager: hidden zones release their space; visible zones reflow to fill (left region = vertical stack; VARIATIONS = right region — hiding it gives the left full width).
- [x] Transformer window docks directly under the SAMPLE panel (fixed height for now).
- [x] FX rack + effect editor scroll vertically when their zone is squeezed (so reflow never hides controls).
- [ ] Transformer height grows with the number of visible lanes (showing all 8 shunts FX down) — deferred refinement.
- [ ] Persist the visible-zone set so the layout sticks across launches — deferred.

**Part B — Transformers** (parameter modulation):
- [x] Transformer model (×8): target = an effect slot+param, **pre-effect sample amplitude**, or **post-effect output amplitude**; plus shape, depth (bipolar), time-basis (one-shot / cyclic), on/off.
- [x] Shapes: presets (ramp up/down, triangle, sine, saw, square, exp decay, random S&H) **plus user-drawn**; editing any preset converts it to the user-drawn shape.
- [x] Transformer panel under SAMPLE: per-lane shape graph (time × value) with click-drag to draw; compact target / shape / depth / rate controls; `1 … 8` lane buttons select + enable.
- [x] Cyclic transformers expose **Freq** (free-Hz, log-mapped) + **Phase**; rate combo adds a `Free` option (Freq active only in Free mode). One-shot hides them.
- [x] Transformer graph draws the tempo/beat grid (one-shot spans the output and aligns with the sample lanes; cyclic spans one cycle).
- [x] Modulation engine: during render, evaluate each transformer over the output timeline and apply to its target parameter.
- [x] **Effects → stateful block processors:** effects process in small blocks with per-block (modulated) params, carrying filter/delay state across blocks.
- [x] Modulated render stays deterministic and tail-aware; transformer state travels in the rack snapshot (so M4 variations can carry/mutate it).
- [ ] Multi-lane stacked overview (one editable + others as read-only thumbnails) + dynamic `Trans1 … Trans8` count dock — deferred refinement.

**Design notes:**
- Time-basis: *one-shot* maps the shape across the whole rendered output (region + tail); *cyclic* repeats at a rate (Hz or tempo division), aligning with the OUTPUT lane's tempo grid.
- Combine: multiple transformers on one param sum (clamped); effective value = knob base + Σ(depth × shape).
- Cost: block processing + per-render modulation eval is heavier; the M3 background render worker already keeps the UI responsive and shows "redrawing" for longer evals.

**Resolved (this planning pass):**
- Targets: continuous effect params + **two amplitude stages** (dry sample level pre-rack, output level post-rack).
- **Excluded targets** (would click/glitch under modulation): discrete/segmented params — Filter Type, Delay Sync, Delay Mode — and buffer-length params — Delay Time. These stay knob/seg-only.
- Time-basis: **both**, chosen per transformer (one-shot over output vs cyclic rate).
- Lanes: **one editable** (draw the curve) + the others as compact read-only overviews.

Decision this unlocks: does on-the-fly modulation make a single sample feel alive, and is show/hide the right density control — before variations multiply everything?

### M3b — Inputs Browser *(find + load + audition)* — ✅ COMPLETE (refinements deferred)

> **Inserted phase.** Slots between M3a and M4. Lettered (`M3b`) to mark it as another out-of-band insertion after M3a; M4–M6 keep their numbers. Companion to the right-side VARIATIONS browser — an **INPUTS** browser that lets the user pick source samples from folders and step through them, instead of going through the file chooser each time.

Goal: a producer points SampleArk at their sample folders once, then **browses and auditions inputs in-app** — click (or arrow through) a file and it loads straight into the SAMPLE panel, ready to play/prep/mutate. The faster the drop→hear→next loop, the more of their library they run through us.

- [x] **INPUTS panel on the right** (mirror of VARIATIONS): folder picker + file list of audio in the current folder (`wav` / `aif` / `aiff`); folders shown as `[+]`, files as `~`.
- [x] **First top-level toggle:** INPUTS is the **first** button in the top-bar show/hide set (order: `INPUTS · SMPL · TRANS · FX · MUT · VARS`); kept as zone index 5 internally (no renumber) but drawn first.
- [x] **Folder navigation:** FOLDER button opens a directory chooser; `[..]` row goes to the parent; click a sub-folder to descend (flat current-folder list, chosen over a recursive tree for v1).
- [x] **Browse + audition:** click a file to load it into the SAMPLE panel (same path as LOAD SAMPLE); Up/Down step through the files and auto-load, so `P` auditions immediately.
- [x] **Right-region sharing:** INPUTS + VARIATIONS split the right region (INPUTS top, VARIATIONS bottom) when both shown; either alone takes the full right region; neither gives the left stack full width.
- [x] Selection is **non-destructive**: browsing/loading only sets the source; loaded file is highlighted. Wheel scroll + scrollbar thumb.
- [ ] Persist the chosen folder + last-open folder across launches — deferred (defaults to the user Music folder; first listing of a protected folder triggers the macOS privacy prompt once).
- [ ] Waveform thumbnails / duration per input row — deferred (name-only for v1).

Decision this unlocks: does an in-app library browser make the drop→hear→next loop fast enough to keep producers inside SampleArk, before variations multiply the output side?

### M4 — The Magic Moment: Variations *(hear + choose + export)* — ✅ COMPLETE

Goal: the promise — drop a sample, get a batch of useful variations, pick favourites, write them out.

- [x] Variation model that captures enough state to re-render itself (trim, gain, rack snapshot + transformers, seed, scope, level, selected, export name) — `Variation` in `Variations.h`.
- [x] Mutate: generate a batch of candidates (16, first 8 selected) from intensity + scope; each re-rendered off the live base via `renderState`.
- [x] Mutation scope toggles (Everything / Filter / Mangle / Tail / Timing / Pitch / Dynamics / Crossfades / Plug-ins) wired to the bottom MUTATE strip.
- [x] Transformer curves mutate too (the ON lanes): Gentle = coherent ±5–10% whole-curve drift; Vibing/Massive/Unsafe add a smooth random difference curve (≈±5 / ±10 / ±25%), clamped 0..1.
- [x] Variation browser: per-row audition, mini-waveform, name, select/favourite, mute.
- [x] **Recall + Baseline:** row 00 = "Baseline" (the unvaried state the user set, the undo anchor); clicking any row loads its recipe (prep + rack + transformers) into the live editors + OUTPUT and marks it the active/LIVE row. Favourite is a separate gesture, capped at 8. Writing clears the RAM stack (files on disk become the only memory).
- [x] Audition the list like a playlist, with a ~1-second pause between samples. **PLAY ALL** button (replaced KEEP PLAYING): auditions every non-muted entry in turn, recalls each into the rack, loops until stopped.
- [x] Select favourites; show favourites count + cap (e.g. `8 / 8`).
- [x] **Write** selected favourites to a new folder with sensible naming + a manifest (24-bit WAV + `manifest.txt`).
- [x] Click-free endings: raised-cosine output fade (5 ms min) so written one-shots never pop in other apps.
- [x] **Manual capture (added later):** "+ ADD THIS" snapshots the live settings as a new variation; stable per-variation numbering (00 = Baseline); MUTATE asks Add-to-existing vs Replace-all so hand-built and mutated candidates coexist.
- [ ] Trial output-count limiter (counts exports without limiting preview/generation). *(deferred — licensing concern, not core loop.)*

Decision this unlocks: does "8 useful variations in ~30 seconds" actually feel real? This is the build that validates or kills the product thesis.

### M5 — Full Rack + Mutation Depth *(change, deeper)* — ✅ COMPLETE

Goal: complete the built-in stack and make mutation genuinely steerable, not random.

- [x] Remaining built-ins: Compression, Reverb, Limiter, Autopan — real DSP + per-effect editor graphs, processed in the stateful block chain like the M3 effects.
- [x] Tail-aware rendering so delay/reverb tails print correctly (shared M3a tail handling; reverb now rings into it).
- [x] Mutation levels: Gentle, Vibing, Massive, Unsafe — named zones on one continuous severity scale, with per-parameter ranges (gentle stays musical; unsafe can break things on purpose). `zoneMag` + `musicalRange` in Variations.cpp; Unsafe expands into the danger zone (e.g. reso self-oscillation).
- [x] Per-parameter mutation metadata; gate order/bypass mutation to higher levels. Finer scope categories (`categoryScope`); bypass/reorder/type-switch gated to Massive+. MUTATE sublabel echoes zone + %.
- [!] Blueprints: built as a SETUPS row of 8 mutation-mode buttons (depth + affects + rack snapshot), then **removed at the user's request** — storing *mutation behaviour* there was confusing; it read like an FX/trim preset but changed how things mutate. **Open question for Arch07:** a proper whole-sound **preset** system (FX + trim + rack settings, named, saved/recalled), kept separate from the mutation controls. Not a blocker for M5's intent (steerable mutation), which is met.

> Note: the **rack-completion** half of M5 landed early (alongside M3a/M3b). The **mutation-depth** half attaches to the M4 variations/mutation engine and will be built with M4.

Decision this unlocks: are clean mutations reliably useful and wild/unsafe deliberately strange — i.e. a controllable search, not blind randomness?

### M6 — Tempo & Warp *(change, loops)* — ⬜ NOT STARTED (the last v1 milestone)

Goal: useful tempo conversion without turning into a DAW.

> Status: nothing built. The toolbar shows a **static** TEMPO 120 chip + DETECT button (placeholders); transformers already use tempo for cyclic rates, but tempo is fixed at 120 and not user-settable, and there is no time-stretch/warp engine yet.

- [ ] Global tempo per session (default 120 BPM), user-settable.
- [ ] Old-school resampling (speed and pitch linked) — musically valuable and technically simpler; do this first.
- [ ] Tempo-detection spike for loop material (always editable).
- [ ] Swappable `TimeStretchEngine` interface so a pitch-preserving engine can drop in later.
- [ ] Evaluate Rubber Band / SoundTouch / Signalsmith against an Ableton-level quality bar by ear; if none clears it, ship old-school only and defer modern warp.

Decision this unlocks: is modern pitch-preserving warp good enough to ship, or do we keep old-school and defer?

## Post-Plan Enhancements (built on request, beyond the milestone list) — ✅ DONE

These were added during M4/M5 work in response to live feedback. All complete and pushed.

- [x] **Live modulation feedback** — the selected effect's editor graph morphs at the playhead while playing (`~ LIVE` tag), mirroring the transformer modulation that's baked into the render.
- [x] **Transformer fixes** — curves mutate; transformers follow their effect on reorder (slot remap); one-shot sweep spans the whole output (region + tail) and the chart matches the audio; ON-but-inactive lanes shown in faded orange.
- [x] **Dynamics meters** (Compression + Limiter) — output-level (dBFS) + gain-reduction (dB) meters with peak-hold ballistics, red clip glow within 2 dB of 0 dBFS, captured offline during render and read at the playhead.
- [x] **Makeup gain** on Compression + Limiter (final stage, pre-meter); **Limiter ceiling now in dB**.
- [x] **Limiter excluded from mutation** — always left untouched as a safety net (params, bypass, reorder-crossing, and modulation all preserved).
- [x] **Audio-interface selector** — AUDIO toolbar button opens the standard JUCE device picker (output device / sample rate / buffer size) on the live engine.
- [x] **App icon** — `sampleark-app/app/icon.png`, embedded via CMake `ICON_BIG` so the bundle ships `Icon.icns`.
- [x] **Permissions allow-list** for smoother local dev (`.claude/settings*.json`).

## Deferred — Post-v1 (do not start until the M1–M4 loop is proven)

These are real, but they are not what makes or breaks v1. Each is a known risk/time sink.

- **AU/VST3 hosting** — scanner, registry, hosted processor + UI, state serialization, crash isolation. Target v1.5. The built-in rack must stand alone without it.
- **Advanced rack containers** — any slot becomes a container with serial chains and parallel lanes, lane gain/bypass, safe summing/headroom, collapse/expand UI. Powerful, but risks making the UI feel modular too early.
- **AI mutation research** — candidate generator only, hidden behind an admin/lab surface, biased toward isolated-source generation and sample transformation over full-mix generation. Research lane, not a v1 promise.
- **Web funnel** — marketing site, before/after audio, native-trial download. Explicitly *not* a browser clone of the engine (a worse-sounding demo would mis-sell the product).

## Cross-Cutting: Audio Quality Tests

Verified informally (by ear + ad-hoc numeric self-tests during development), but **no automated test suite exists yet** — this is the biggest piece of technical debt. Worth a dedicated pass (Arch07 candidate).

- [~] Null/cancellation tests for neutral operations. *(informal only)*
- [~] Preview/render consistency tests. *(preview plays the same buffer that's written — parity by construction; not formally tested)*
- [~] Boundary click tests. *(addressed via raised-cosine fades; verified by ear)*
- [~] Tail rendering tests. *(delay/reverb tails verified by ear + waveform)*
- [~] Gain consistency across variation exports. *(not formally tested)*
- [ ] Sample-rate conversion tests. *(N/A until M6 resampling lands)*
- [ ] Dither application tests. *(N/A until multi-bit-depth export is surfaced)*
- [ ] (Later) parallel-lane summing and plug-in latency compensation tests.

## Immediate Decisions (needed to start M0) — resolved

- [x] **App name / package id.** `sampleark` is the working code identifier; bundle id `com.sampleark.app`.
- [x] **Folder layout.** Nested `sampleark-app/` with `core/`, `app/`, `vendor/` (JUCE), `build-native/`; Arch docs at repo root.
- [~] **Target platforms.** Mac + Windows from one codebase was the intent, **but only Mac has actually been built/run so far** — Windows configure/build is unvalidated (see What's left).
- [x] **One set of screens / no view toggle.** Decided: there is no Simple/Complex toggle. The top-right SIMPLE/COMPLEX control in the mockups is an artifact and is dropped. Complexity is disclosed by folding in place.
- [x] **Action terminology.** Decided: follow Arch01 — **Mutate** (re-roll candidates) and **Write** (commit favourites to a folder). The handoff's GENERATE / PRINT SELECTED labels are not used.
- [x] **AU/VST swap affordance in v1.** Decided: keep the per-slot `⇄` swap and CORE/VST badges visible but **inert and labelled "v1.5"** in v1, so the rack layout doesn't shift when hosting lands.
- [ ] **Trial output allowance** (e.g. 16 × 8). Can be decided by M4; flag it now so the limiter is designed in.

## What's Left (open items as of 2026-06-30) — for review / possible Arch07

Nothing below blocks the core loop, which works. Grouped by size.

**Next milestone**
- **M6 — Tempo & Warp** (not started): user-settable session tempo, old-school resampling first, tempo-detect spike, swappable `TimeStretchEngine`, quality bake-off for pitch-preserving warp. The TEMPO chip + DETECT button are currently static placeholders.

**Notable gaps / debt**
- **Windows build unvalidated.** Developed and run on Mac only; the cross-platform promise needs a real Windows configure/build/run pass.
- **No automated audio-quality test suite.** Only informal/by-ear verification so far (see Cross-Cutting). Biggest technical debt.
- **Export bit-depth not surfaced.** Prepped export and variation write are 24-bit; the 16/32-bit + dither paths from M2 aren't exposed in UI. (Arch05/06 envisioned global format defaults in Preferences — no Preferences screen exists yet.)
- **No Preferences / Config screen.** Several specs assume one (export defaults; the "Config button in the title bar" idea). Not built.

**Deliberately deferred refinements (small)**
- M2: better transient/start auto-detect.
- M3a: transformer dock height grows with visible lane count; multi-lane stacked overview; persist the visible-zone set across launches.
- M3b: persist chosen/last folder across launches; waveform thumbnails + duration per input row.
- M4: trial output-count export limiter.
- Variations: PLAY ALL loops continuously rather than stopping after one pass (toggle could be offered).
- DEPTH slider: click-a-zone-to-snap (drag works today).

**Open design question (the Blueprints fallout)**
- A proper **whole-sound preset** system — save/recall **FX + trim + rack settings** as named presets, kept **separate** from the mutation controls. (The removed SETUPS row conflated this with mutation behaviour.) Good Arch07 candidate.

**Post-v1 (unchanged):** AU/VST3 hosting · advanced rack containers (serial/parallel) · AI mutation research · web funnel.

## Notes

- Keep Arch03 practical and editable. Update checkboxes as real, on-disk work lands — and only when it lands.
- Do not record work as complete unless it exists in the repository and runs.
- If a milestone grows too large, split it into a `BuildXX_` task file rather than bloating this tracker.
</content>
</invoke>
