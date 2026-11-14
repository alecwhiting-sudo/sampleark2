# Arch07 Next-Phase Plan

## Purpose

Forward tracker for the work remaining after **M0–M5 shipped** (see `Arch03_Implementation_Plan.md` → *Current Build Status* and *What's Left*). The core loop is proven and runs: browse/drop a sample → see + hear → prep + rack + transformers → mutate (or hand-capture) variations → recall/audition/playlist → favourite → write to disk.

This doc collects everything still open into one place so it can be sequenced another day. Nothing here blocks the working loop; it's about finishing v1 and paying down debt.

Companion assessment: **`Arch08_Editor_Feature_Gap.md`** (SampleArk vs classic Sound Forge on basic sample editing, with per-feature cost bands). Nothing from it has been adopted into the priorities below yet — that's a deliberate decision to take, not a backlog to absorb.

## Status Key

- `[ ]` Not started · `[~]` In progress · `[x]` Complete · `[!]` Blocked / needs decision.

## Priorities at a glance

| # | Item | Size | Why now |
|---|---|---|---|
| P1 | **M6 — Tempo & Warp** | Large | The last v1 milestone. **In progress:** tempo, Resample, DETECT, engine interface and a first Warp candidate (Signalsmith) done; the by-ear verdict is left. |
| P2 | **Whole-sound preset system** | Medium | The thing "setups" was meant to be; replaces removed Blueprints. |
| P3 | **Preferences / Config screen** | Medium | Unlocks export bit-depth + format defaults several specs assume. |
| P4 | **Automated audio-quality test suite** | Medium | Biggest technical debt; protects determinism/parity as we add M6. |
| P5 | **Windows build validation** | Medium | The cross-platform promise is currently Mac-only. |
| P6 | **Small deferred refinements** | Small | Polish; pick up opportunistically. |

P1–P5 are independent enough to run in any order; P4 ideally lands alongside or just before P1 so warp doesn't regress audio quality silently.

### Shipped since this doc was written

- **[x] Undo / redo** (first item adopted from Arch08). 100-step history over the editor-state triple (prep + rack + transformers) — the same triple `applyRecipe` swaps and a variation snapshots — with `Cmd+Z` / `Shift+Cmd+Z` / `Cmd+Y`, toolbar buttons that dim when empty, and the step name flashed in the filename chip. Continuous gestures coalesce to one step (600 ms window, per-target tag); no-op changes are rejected so a step never restores an identical state; variation **Recall** is undoable; history clears on sample load. Tests: `sampleark-app/tests/undo_history_test.cpp` (31 assertions). Spec: Arch06 → *Undo / redo*.
  - Fixed alongside: the effect editor now rebuilds when the built slot holds a **different effect type** (undo, redo and recall can all reorder the rack under it — a latent out-of-bounds read on the parameter list before this).
  - Not persisted across sessions; no history *window* (Sound Forge has one) — the flash is the only trace of what moved.

- **[x] Tail control (Delay + Reverb).** Per-slot `OFF / FULL / TIME / xLOOP` tail mode + amount, deciding how far each tail-producing effect may extend the sample past the trimmed region. Output-length policy only — the natural ring-out is always rendered and retained (bounded pristine buffer, no output fade baked in), so the material stays available for the features below. Exact modes pad rather than trim. Published length = region + longest grant; the OUTPUT lane ghosts the cut ring-out behind a cut line, and the SAMPLE header reads `OUT ... (region + tail)`. Never mutated; inherited by variations, recall and capture. Spec: Arch06 → *TAIL row*, Arch05 → *Never mutated*.
  - Unlocked by the retained tail, not yet built: wrap the tail back into the loop start for seamless loops · audition/write the tail as its own layer · flip tail on/off with no re-render · a `SYNC` tail mode (bars/beats) once P1 lands tempo.
  - Not persisted across sessions (same as every other rack setting — see P3).

---

## P1 — M6: Tempo & Warp *(change, loops)* — `[~]` in progress

Goal (from Arch03): useful tempo conversion without turning into a DAW.

- [x] Global **session tempo** (default 120 BPM, 40–220). The TEMPO chip is live: `−`/`+` steppers, drag or scroll the value (Shift = 0.1), double-click to type. `AudioEngine::setTempo` re-renders, so synced Delay, cyclic transformers, PLAY EVERY, the grid and Follow all move. Session state: not undoable, not mutated, not stored in a variation.
- [x] **Old-school resampling** (speed + pitch linked). PITCH tab: Transpose (±24 st) + Tune (±100 ct), plus **Loop BPM** (the region's own tempo) and **Follow** (conform it to the session tempo). Sits after the source fades, before the rack (Arch06 chain). Band-limited Kaiser-windowed sinc — speeding up doesn't alias; rate exactly 1 is a straight copy, so a neutral PITCH tab is bit-identical to before. Buffers grow on demand when a region is slowed down (rate clamped to 1/8..8). Preview == print verified sample-for-sample against `renderState`.
- [x] **Tempo detection** for loop material — DETECT. Onset-envelope autocorrelation with a comb over beat multiples and a mild ~128 BPM prior, then **length-locked**: if the region is a whole number of bars (or beats) within a few percent, the exact length-derived tempo wins, so a Follow loop never drifts. One-shots and silence report "no clear tempo". DETECT sets Loop BPM + Follow and moves the session tempo to match (sound unchanged, grid now on the hits); pressing it again on the same region steps through the octave alternatives (87 ↔ 174). One undo step. The SOURCE grid draws at the loop tempo from the trim start once Loop BPM is known.
- [x] Swappable **`TimeStretchEngine`** interface — `sampleark_core` (`TimeStretch.h`), headless and JUCE-free. `ResampleEngine` is the first implementation; `warpEngine()` is the slot a pitch-preserving candidate plugs into, picked at build time with the CMake option `SAMPLEARK_WARP` (`signalsmith` default, `none` = Resample only, Warp dimmed).
- [x] **Warp candidate #1: Signalsmith Stretch 1.4.0** (MIT, vendored in `sampleark-app/third_party/` with its `linear` 0.6.4 dependency). `core/src/SignalsmithWarp.cpp`: whole-region offline `exact()` call (output exactly the requested length, hits aligned — tested within 10 ms), fixed-seed so renders are deterministic, short one-shots zero-padded past the stretcher's 120 ms window. Warp + Follow changes tempo with pitch untouched; Transpose/Tune keep the length; **Formant** is live in Warp (±12 st, pitch-compensated, so 0 = no formant processing). Preview == print verified. A 7 s stereo loop renders in ~0.1 s (the DSP sources are built `-O2` even in Debug; ~1.7 s unoptimised). Resample stays the default mode until the verdict below.
- [x] **Pitch mutation** (Arch05 matrix): Gentle ±10 ct · Vibing ±1 st · Massive ±2–7 st · Unsafe octave jumps; formant at Massive+ (Warp only). A region that is Follow-locked in Resample mode is never pitch-mutated — there, pitch *is* tempo and a detuned candidate would drift off the grid.
- [ ] **Quality bake-off — by ear.** Signalsmith is in and switchable against Resample in the PITCH tab. Listen on drums (transient smear at ±10–30 % tempo), bass (phasiness), vocals/pads (formants), against an Ableton-level bar. Measured so far: pitch accurate to <1 Hz at +12 st, level within 0.3 dB. Upstream says it is best between 0.75× and 1.5× stretch. If it fails, the next candidates are Rubber Band (GPL/commercial — affects selling the app) and SoundTouch (LGPL); each is one more `TimeStretchEngine` subclass + a `SAMPLEARK_WARP` value. If none clears the bar, build with `SAMPLEARK_WARP=none` and ship old-school only.
- [ ] `SYNC` tail mode (bars/beats) for Delay/Reverb tails, now that tempo is live (from the tail-control notes).

Decision this unlocks: is pitch-preserving warp good enough to ship, or keep old-school and defer?

Known limits: detection is tuned on drum loops — sparse or ambient material may need the octave cycle or a typed value; tempo/loop BPM aren't persisted across sessions (P3); session tempo is not saved with a variation (WRITE renders at the tempo in force).

## P2 — Whole-Sound Preset System *(the "setups" the user actually wanted)*

Background: an 8-slot "SETUPS" row was built in the MUTATE strip (Blueprints) and **removed** — it stored mutation behaviour (depth + affects), which read like an FX/trim preset but changed how things *mutate*. The real want is a preset of the **sound**, kept clearly separate from the mutation controls.

- [!] **Decide placement** — not in the MUTATE strip. Candidates: a row in/near the PREP or FX RACK area, or a small preset bar in the toolbar. (Needs a UI decision.)
- [ ] A preset stores **prep (trim/shape/pitch) + full FX rack + transformers** — i.e. `applyRecipe`'s payload — but **not** depth/affects.
- [ ] Click recalls the preset into the live rack (reuse `engine.applyRecipe`); a clear save gesture (e.g. shift/right-click, matching the old idiom) stores the current sound.
- [ ] Name + manage slots; show which slot is active.
- [ ] **Cross-session persistence** — save presets to a settings file so they survive restarts (Blueprints were in-memory only; do this properly here).

Note: this and the manual-capture variations ("+ ADD THIS") are complementary — capture builds a *variation list*; presets save a *reusable sound*.

## P3 — Preferences / Config Screen

Several specs (Arch05/Arch06) assume a Preferences surface; none exists. Also the "Config button in the title bar" idea.

- [ ] A Preferences window (or title-bar Config button) hosting global settings.
- [ ] **Export format defaults** — bit depth (16 / 24 / 32-float), dither on the fixed-point paths, sample-rate policy. Wire into the prepped export and the variation write (both currently hard-coded to 24-bit).
- [ ] Surface other latent settings as they arise (default folders, favourite cap, batch size).

Unblocks: the M2 multi-bit-depth + dither export paths that exist in spirit but aren't exposed.

## P4 — Automated Audio-Quality Test Suite

Biggest technical debt — only informal/by-ear checks exist. Build against the headless `core` where possible so it runs without a UI.

- [ ] Null/cancellation tests for neutral operations (bypassed rack == passthrough).
- [ ] Preview/render parity tests (same algorithm, sample-accurate).
- [ ] Determinism tests (a fixed variation state renders identical audio every run).
- [ ] Boundary-click tests (fades leave no discontinuity; check endpoints).
- [ ] Tail-rendering tests (delay/reverb tails print fully, trim to silence correctly).
- [x] **Tests are wired into CMake/CTest** (`ctest --test-dir build-native`). `core_tempo_warp` links only `sampleark_core`; `tail_policy_test`, `undo_history_test` and `tempo_warp_engine_test` compile the engine sources into JUCE console apps and drive the real render path. All four pass on macOS and Linux (no audio device or display needed) — the Linux build is a first step towards P5's "not Mac-only".
- [~] Started by M6: a null test (neutral PITCH + bypassed rack == source), a preview/print parity test (live worker render == `renderState`, sample for sample, with pitch active), and a determinism test — all in `tempo_warp_engine_test`. The general versions (any rack state) are still to do.
- [ ] Gain-consistency tests across variation exports.
- [~] Sample-rate-conversion tests: the resampler's pitch accuracy, unity gain, length and anti-aliasing are covered in `core_tempo_warp`. Dither tests wait for P3.

## P5 — Windows Build Validation

The cross-platform promise (Arch03 invariant) is currently **Mac-only**.

- [ ] Configure + build the CMake project on Windows (MSVC).
- [ ] Run the app: device IO, file load, render, playback, write.
- [ ] Fix any platform-specific issues (paths, fonts, device manager defaults, icon).
- [ ] Ideally a CI job so it doesn't regress.

## P6 — Small Deferred Refinements

Opportunistic polish; none blocks anything.

- [ ] M2: better transient/start auto-detect for percussive material.
- [ ] M3a: transformer dock height grows with visible lane count; multi-lane stacked overview (one editable + read-only thumbnails); persist the visible-zone set across launches.
- [ ] M3b: persist chosen/last input folder across launches; waveform thumbnails + duration per input row.
- [ ] M4: trial output-count export limiter (counts exports without limiting preview/generation).
- [ ] Variations: offer **PLAY ALL** play-once vs the current continuous loop.
- [ ] MUTATE: DEPTH slider **click-a-zone-to-snap** (drag works today).
- [ ] Tail: rack-row marker so a tail-off Delay/Reverb is visible without opening its editor.
- [ ] Tail: musical cut fade (the cut currently uses the 5 ms safety fade; a longer auto-fade when truncation removes audible material is an option if hard cuts sound abrupt).

---

## Post-v1 (unchanged — do not start until v1 is proven)

- **AU/VST3 hosting** — scanner, registry, hosted processor + UI, state serialization, crash isolation. Target v1.5; the built-in rack stands alone without it.
- **Advanced rack containers** — serial chains + parallel lanes per slot, lane gain/bypass, safe summing, collapse/expand.
- **AI mutation research** — candidate generator behind an admin/lab surface; research lane, not a v1 promise.
- **Web funnel** — marketing site with before/after audio + native-trial download; not a browser clone of the engine.

## Notes

- Same rules as Arch03: tick items only when they exist in the repo and run; split anything that grows large into its own `BuildXX_` task file.
- Cross-references: build status + history in `Arch03`; mutation model in `Arch05`; UI control map in `Arch06`; design tokens/layout in `Arch04`.
