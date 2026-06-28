# WIP — UI → Technical Requirements Mapping (working notes)

> **TEMPORARY SCRATCH DOC.** Started 2026-06-27. This is where we work out the
> mutation matrix, scope definitions, and the UI-element → requirements mapping
> before folding it into formal docs (`Arch05_Mutation_Spec.md` + `Arch06_UI_Requirements.md`).
> Not authoritative yet. Pick up at **"Next actions"** at the bottom.

---

## Decisions locked this session

- Platforms: Mac + Windows together, one codebase.
- **No** Simple/Complex toggle — one set of screens, fold-in-place.
- Action names: **Mutate** + **Write** (not Generate/Print).
- AU/VST hosting deferred to v1.5; per-slot `⇄` swap stays visible but inert.
- Mutation levels renamed canonical: **Gentle / Vibing / Massive / Unsafe** (was Clean/Push/Wild/Unsafe). Done in Arch02/Arch03.
- **Depth == severity:** one continuous mutation-severity value; Gentle/Vibing/Massive/Unsafe are named zones along it. UI is one slider (drag for fine, click a zone to snap). The old separate "amount" knob is gone.
- **MUTATE module:** depth slider funnels down into the AFFECTS chips (one driver→targets unit).
- **Dynamics** added as a 10th MUTATE AFFECTS facet (compressor + limiter).

## Project state

- M0 = static main-screen shell, builds + runs on Mac. Committed locally as `d08dfa3` ("M0: project scaffold + static main-screen shell").
- **Push pending:** GitHub auth not available non-interactively. User to run `git push -u origin main` once (origin = github.com/alecwhiting-sudo/sampleark2), then future pushes work via keychain.
- Build/run: see `sampleark-app/README.md`. App: `sampleark-app/app/src/Panels.cpp` holds the static zones.

---

## Mutation model (current proposal — to finalise)

### Depth levels (one severity scale, 0–100%)

| Zone | ~range | Magnitude | Structural ops (bypass / reorder / type-switch) | Character |
|---|---|---|---|---|
| **Gentle** | 0–25% | ±0–10% of musical range | none | always usable, subtle |
| **Vibing** | 25–50% | ±10–30% | rare, only where musical | groove, still safe |
| **Massive** | 50–75% | ±30–70%, full musical range | yes | dramatic, renders clean |
| **Unsafe** | 75–100% | full 0–100% incl. danger zones | yes + random | self-osc, clip, glitch, deliberate breakage |

### Scope (AFFECTS) definitions — what each facet touches

| Facet | Technically | Params |
|---|---|---|
| Everything | master; enables all below | all |
| Filter | spectral shaping | Filter cutoff/reso/type; tone controls on other FX |
| Envelopes | amplitude-over-time | Shape Attack/Hold/Release; comp time-constants |
| **Dynamics** *(new)* | level control | Compressor thresh/ratio, Limiter ceiling |
| Crossfades | boundary fades / click protection | Fade In/Out length + curve |
| Timing | time placement & sync | Trim start/end pos, Delay time, Autopan rate |
| Pitch | tuning & pitch | Tune, Transpose, Formant, warp/resample mode |
| Mangle | destructive timbral character | Distortion drive, Bitcrush bits/rate, waveshaping |
| Tail | space & decay | Reverb size/decay/mix, Delay feedback/mix |
| Plug-ins | catch-all for remaining rack params; in v1.5 hosted AU/VST params | Mix amounts, misc; (v1.5) plugin params |

### Depth × Scope matrix (concrete behaviour — NEEDS USER REVIEW of magnitudes)

| Scope | Gentle | Vibing | Massive | Unsafe |
|---|---|---|---|---|
| Filter | ±5% cutoff, tiny reso | ±20% cutoff, mild reso | wide sweep, type may switch | self-oscillating reso, extreme cutoff |
| Envelopes | ±5% A/H/R | ±20%, snappier/looser | punch↔pad reshapes | zero-attack clicks, infinite hold |
| Dynamics | ±5% thresh/ratio | ±20%, more pump | hard squash / heavy limit | over-compress, pumping/distortion |
| Crossfades | ±few ms | ±10–20 ms | long fades / near-gate | 0 ms (clicks) or fade eats sample |
| Timing | ±2 ms start nudge | ±10 ms, slight delay shift | start jumps, delay re-times | start past transient, runaway delay |
| Pitch | ±10 cents | ±1 semitone | ±5–7 semis, formant shift | octaves, alias, formant break |
| Mangle | barely-there grit | audible drive/crush | heavy saturation / 8-bit | full-destroy, fold, gated noise |
| Tail | ±5% size/decay | ±25%, more space | huge verb / long feedback | infinite feedback, runaway wash |
| Plug-ins | ±5% safe params | ±20% | structural toggles/reorder | random bypass + random order |

> Mechanism: each in-scope parameter carries `category` + per-zone range metadata
> (Arch02 `ParameterMutationSpec`). Engine re-samples each in-scope param within
> the range its current severity zone allows. Deterministic per seed.

---

## UI element → requirements gap inventory

Status: ✅ mapped · 🟡 visual-only/needs eng values · 🟠 on UI, undefined · 🔴 in spec, no UI home.

### 🔴 In spec but NO UI home (need product decisions)
- **Export/Write settings** — bit depth (16/24/32f), sample rate, dither, normalize, tail length, name prefix/suffix. Only `WRITE SELECTED →` exists. → needs a Write panel/popover.
- **Destination folder** — "write to a new folder" picked where? No picker.
- **Blueprints / 8 favourite chains** — Arch01/02 want 8 favourite blueprint buttons *beside the mutation controls*; UI has none (chips occupy that space). Design conflict to resolve.
- **Trial limiter** — remaining export allowance (e.g. 16×8) not shown anywhere.

### 🟠 On UI, not technically defined (I can just define these)
- PLAY/STOP scope — plays source vs prepped one-shot vs selected variation? (matters)
- "transient-locked" status meaning
- TRIM "Transient" knob — vs Auto-Detect; what param? (sensitivity? offset-to-transient?)
- SHAPE — Gain dB range; Attack/Hold/Release ms + applied how at print; Normalize target (peak vs LUFS)
- PITCH — Tune cents vs Transpose semis; Formant function+range; Warp vs Resample exact behaviour
- Fade In/Out — units (ms) + curve shape
- KEEP PLAYING — purpose; likely redundant (candidate to cut)
- "20 candidates" — generate count fixed or configurable; why 20→8

### 🟡 Visual-only / eng values pending
- Per-effect param ranges/units (Cutoff Hz, drive dB, delay ms/sync) — defer to each effect's build
- Variation naming derivation from source filename

### ✅ Mapped OK
Load/decode, waveform, trim start/end, rack reorder/bypass, 8 built-in effect identities, depth=severity model, audition playlist (1s gap), determinism/seed, WAV export quality rules.

---

## Next actions (pick up tomorrow)
1. **Finalise the mutation matrix** above — user to react to magnitudes; then I write it into `Arch05_Mutation_Spec.md`.
2. **Resolve the 🔴 product decisions:** where export settings + destination folder live; whether/where blueprints get a surface (vs the mutation chips).
3. **Define the 🟠 list** (PLAY scope, Transient knob, SHAPE/PITCH units, KEEP PLAYING keep-or-cut, candidate count).
4. Fold this WIP into formal `Arch05_Mutation_Spec.md` + `Arch06_UI_Requirements.md`, then delete this file.
5. User: run `git push -u origin main` to publish M0.
</content>
