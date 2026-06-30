# Arch07 Next-Phase Plan

## Purpose

Forward tracker for the work remaining after **M0–M5 shipped** (see `Arch03_Implementation_Plan.md` → *Current Build Status* and *What's Left*). The core loop is proven and runs: browse/drop a sample → see + hear → prep + rack + transformers → mutate (or hand-capture) variations → recall/audition/playlist → favourite → write to disk.

This doc collects everything still open into one place so it can be sequenced another day. Nothing here blocks the working loop; it's about finishing v1 and paying down debt.

## Status Key

- `[ ]` Not started · `[~]` In progress · `[x]` Complete · `[!]` Blocked / needs decision.

## Priorities at a glance

| # | Item | Size | Why now |
|---|---|---|---|
| P1 | **M6 — Tempo & Warp** | Large | The last v1 milestone; only static placeholders today. |
| P2 | **Whole-sound preset system** | Medium | The thing "setups" was meant to be; replaces removed Blueprints. |
| P3 | **Preferences / Config screen** | Medium | Unlocks export bit-depth + format defaults several specs assume. |
| P4 | **Automated audio-quality test suite** | Medium | Biggest technical debt; protects determinism/parity as we add M6. |
| P5 | **Windows build validation** | Medium | The cross-platform promise is currently Mac-only. |
| P6 | **Small deferred refinements** | Small | Polish; pick up opportunistically. |

P1–P5 are independent enough to run in any order; P4 ideally lands alongside or just before P1 so warp doesn't regress audio quality silently.

---

## P1 — M6: Tempo & Warp *(change, loops)*

Goal (from Arch03): useful tempo conversion without turning into a DAW.

- [ ] Global **session tempo** (default 120 BPM), user-settable from the toolbar — wire the currently-static TEMPO chip (typed value + ±1 steppers) into `AudioEngine::tempoBpm`. Cyclic transformers already consume tempo; make it live.
- [ ] **Old-school resampling** (speed + pitch linked) first — musically valuable, technically simplest. Render path + preview parity.
- [ ] **Tempo-detection spike** for loop material; result always editable. Wire the static DETECT button.
- [ ] Swappable **`TimeStretchEngine`** interface so a pitch-preserving engine can drop in without touching the render core.
- [ ] **Quality bake-off:** Rubber Band / SoundTouch / Signalsmith against an Ableton-level bar, by ear. If none clears it, ship old-school only and defer modern warp.

Decision this unlocks: is pitch-preserving warp good enough to ship, or keep old-school and defer?

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
- [ ] Gain-consistency tests across variation exports.
- [ ] (When M6 lands) sample-rate-conversion + dither-application tests.

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

---

## Post-v1 (unchanged — do not start until v1 is proven)

- **AU/VST3 hosting** — scanner, registry, hosted processor + UI, state serialization, crash isolation. Target v1.5; the built-in rack stands alone without it.
- **Advanced rack containers** — serial chains + parallel lanes per slot, lane gain/bypass, safe summing, collapse/expand.
- **AI mutation research** — candidate generator behind an admin/lab surface; research lane, not a v1 promise.
- **Web funnel** — marketing site with before/after audio + native-trial download; not a browser clone of the engine.

## Notes

- Same rules as Arch03: tick items only when they exist in the repo and run; split anything that grows large into its own `BuildXX_` task file.
- Cross-references: build status + history in `Arch03`; mutation model in `Arch05`; UI control map in `Arch06`; design tokens/layout in `Arch04`.
