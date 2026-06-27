# Arch04 Design Spec

## Purpose

This is the self-contained design and interaction spec for the SampleArk main screen. It consolidates the `design_handoff_sampleark/` Claude-design handoff (the `SampleArk v2.dc.html` prototype, its README, and the PNG mockups in `UI mock ups input only`) into one reference that lives alongside Arch01–03.

It exists so we never depend on opening the prototype's custom `.dc.html` runtime or attaching that folder. **This document is the authoritative UI spec.** The prototype remains as a browsable visual reference only.

All values below are extracted from the prototype's component logic. They are a high-fidelity starting point, not gospel — adapt during the native build, but keep the layout, density, hierarchy, and interaction model.

### Adaptations to project decisions (these override the handoff)

The handoff predates three decisions in Arch03. Where they conflict, **this section wins**:

- **No Simple/Complex toggle.** Remove the `VIEW SIMPLE | COMPLEX` segmented control from the toolbar and the `view` state entirely. All controls are always visible. The handoff's "core control" concept (an orange plate marking which controls survive Simple view) is dropped *as a view mechanism*; the orange accent remains as ordinary visual language. Progressive disclosure happens by folding panels in place, never by a global mode.
- **Mutate / Write, not Generate / Print.** The big candidate-batch button is **MUTATE** (re-roll candidates). The commit-to-folder button is **WRITE SELECTED →**, not "PRINT SELECTED". ("Keep Playing" stays.)
- **AU/VST swap is inert in v1.** Keep the per-slot `⇄` swap affordance and the CORE/VST badge in the layout, but render them disabled/"v1.5" until hosting lands (Arch03 Post-v1). Layout must not shift when hosting arrives.

---

## Window & Layout

- Target desktop window **1380 × 860**, landscape, no menu bar (hard requirement: "no menu diving" — everything lives on the surface).
- Vertical stack: **Title bar (30px)** → **Toolbar (48px)** → **Body (fills rest)**.
- Body is two side-by-side regions:
  - **Left region — 64% width.** Vertical stack, `overflow-y:auto`: Source waveform → Prep strip → (FX Rack + Effect Detail, side by side) → Mutate-affects strip.
  - **Right region — 36% width (flex:1).** Variations list (scrolls) + Mutate/Write footer (pinned bottom).

The right-hand variations region is persistent — generated outputs are the product, so the user always sees where candidates live and how many are selected.

---

## Design Tokens

### Colour

| Token | Hex | Use |
|---|---|---|
| BG (window) | `#161513` | app background |
| Panel | `#201f1d` | raised panels |
| Panel alt | `#1b1a18` / `#1d1c19` | toolbar, right region, strips |
| Well | `#141312` / `#121110` | waveform / graph / inset wells |
| Border | `#423f3b` | panel borders |
| Border subtle | `#2c2a26` / `#2e2c28` | inset borders |
| Button neutral | `#2b2926` / `#211f1d` | neutral buttons |
| Hairline | `rgba(255,255,255,.05–.07)` | row separators, grid |
| **Accent (burnt orange)** | `#cf6a2c` | primary accent, active, knobs, selection |
| Accent light | `#e0843f` / `#f4a05a` | borders, LED highlight |
| Accent tint | `rgba(207,106,44,.07–.18)` | selected / active fills |
| Play green | `#235e38` border `#2f7a48` text `#dff3e4` | PLAY button |
| Success green | `#3aa35c` | the `N / 8` count when exactly 8 selected |
| Yellow | `#f4be4f` | transient tick |
| Ink | `#e9e7e2` | primary text |
| Dim | `#9a978f` | section labels |
| Faint | `#6f6c65` / `#5f5c56` | meta text |
| Traffic lights | `#ed6a5e` `#f4be4f` `#61c554` | window dots |

### Typography

- **UI font:** system stack — `-apple-system, "Helvetica Neue", system-ui, sans-serif`, antialiased.
- **Mono** (values, filenames, labels — the "scientific" voice): `ui-monospace, "SF Mono", "Roboto Mono", Menlo, monospace`.
- Section labels ~10px, weight 600, letter-spacing 1–1.5px, UPPERCASE. Tiny control labels ~8px. Values 11–13px.

### Spacing / radius / sizes

- Panel padding 10–14px; gaps 8–12px; panel margins 10–12px.
- Radius: panels/buttons **3–4px** (hardware feel, not pill). Knobs & LEDs are circles.
- Knob sizes: **38px** (prep), **42px** (effect detail / mutate dial). FX row **46px**. Variation row **46px**. Toolbar **48px**. Title bar **30px**.
- Shadows minimal. Dragged FX slot: `0 4px 12px rgba(0,0,0,.5)`. LED glow: `0 0 6px rgba(207,106,44,.6)`.

---

## Zones

### Title bar (30px)
macOS traffic lights left; centered mono wordmark "SampleArk"; right `v0.2 · ARCH01`. Gradient `#2b2926→#211f1d`, bottom border `#000`.

### Toolbar (48px, left → right)
- **PLAY** — green: bg `#235e38`, border `#2f7a48`, text `#dff3e4`, 28px tall, `▶` glyph + "PLAY".
- **STOP** — neutral: bg `#2b2926`, border `#423f3b`.
- 1px divider `#3a3833`.
- **DROP SAMPLE** — dashed drop hint: bg `#211f1d`, `1px dashed #4a4640`, orange `⊕` glyph.
- **Filename chip** (centered, mono): bg `#121110`, border `#2e2c28`. Shows `kick_raw_07.wav` · `0.84s` · `48 kHz · 24b`.
- **TEMPO chip:** label "TEMPO", `−`/`+` steppers (range **40–220**, step **1**), mono BPM readout (default **120.0**), **DETECT** button (sets detected tempo; mock value 126.0; always editable).
- ~~VIEW SIMPLE | COMPLEX toggle~~ — **removed** (see Adaptations).

### Source waveform panel
- Panel `#201f1d`, border `#423f3b`, radius 4, padding ~12.
- Header: "SOURCE" (mono caps `#9a978f`) + glowing orange dot; right meta `48 kHz / 24-bit · 40,320 frames · transient-locked` (`#6f6c65`).
- Waveform well: **104px** tall, bg `#141312`, border `#2c2a26`. High-resolution look: ~**230 thin vertical bars, zero gap**, centered/mirrored silhouette. Bars inside trim region `#c8c5bd`; outside trim dim `#403e3a`. Faint center line `rgba(255,255,255,.06)`.
- Overlays: two orange trim handles (2px `#cf6a2c`) at start/end; a yellow transient tick (`#f4be4f`) just after start; a playhead line `rgba(233,231,226,.4)`.
- **Trim handles are driven live by the Trim → Start / End knobs** (and vice-versa once markers are draggable).

### Prep strip
Pre-FX stage. Panel `#201f1d`. Left: "PREP" label + three tabs **TRIM / SHAPE / PITCH** (active = solid orange `#cf6a2c`, text `#1a1410`). Vertical divider. Right: a wrapping row of controls for the active mode. Switching tabs swaps the control set; it does not change anything else on screen.

Controls per mode (knob values are 0..1 defaults from the prototype):

- **TRIM:** Start `0.06`, End `0.92`, Transient `0.62`, Fade In `0.08`, Fade Out `0.18` (knobs) + Auto-Detect (toggle, on).
- **SHAPE:** Gain `0.62`, Attack `0.12`, Hold `0.34`, Release `0.42` (knobs) + Normalize (toggle, on).
- **PITCH:** Tune `0.5`, Transpose `0.5`, Formant `0.5` (knobs) + Time segmented `Warp | Resample` (default Warp).

### FX Rack (left of bottom row, 43% of left region)
Header: "FX RACK" (orange) + `IN → OUT · drag to reorder`. Vertical list of **8 slots**, signal flows top → bottom. Default order:

`1 Distortion · 2 Bitcrush · 3 Compression · 4 Delay · 5 Reverb · 6 Filter · 7 Limiter · 8 Autopan`

Default bypass state: **Compression and Autopan start OFF**; the rest ON. Default selected slot: **Filter**.

Each slot row (46px) left → right:
- **Drag handle** `⠿` (`#5f5c56`, `cursor:grab`) — drag to reorder the chain.
- **Power LED** (13px circle): on = radial `#f4a05a→#cf6a2c` + glow + border `#e0843f`; off = `#1a1916`, border `#3a3833`. Click toggles bypass.
- **Index** number (orange + bold when selected; dim when bypassed).
- **Name** + tiny mono **summary** of the first two params (e.g. `drive 55 · tone 50`, derived from knob value × 100).
- **Badge** `CORE` (built-in, dim) or `VST` (swapped, orange). *(v1: CORE only; VST inert.)*
- **Swap** `⇄` — replaces the built-in with a hosted AU/VST. *(v1: inert/"v1.5".)*
- Selected slot: 3px orange left border + tint `rgba(207,106,44,.09)`. Dragged slot: elevated bg `#2a2724` + shadow.

### Effect Detail panel (right of bottom row, flex:1)
Shows the **selected** slot's purpose-built UI. Header: status dot + effect name + `BUILT-IN` (or `AU / VST`) badge + a hint string.

- **Visual graph** (top, well `#141312`): an SVG plot specific to the effect, drawn in orange `#cf6a2c` over a faint crosshair grid (`rgba(255,255,255,.05)` center lines). Knobs reshape the graph live. Graph math per effect is in the next section.
- **Knob row** (bottom): the effect's parameters as 42px knobs (+ a segmented control for Filter Type).
- If swapped to VST/AU: graph area becomes a diagonal-striped placeholder `3RD-PARTY PLUG-IN · AU / VST · hosted in its own window`. *(v1: this state is unreachable since swap is inert.)*

### Mutate-affects strip (full width of left region)
"MUTATE AFFECTS" label + selectable chips: **Everything · Plug-ins · Filter · Envelopes · Crossfades · Timing · Pitch · Mangle · Tail**.

Exclusivity logic (important): "Everything" is special. When **Everything** is on, the others render as "included" (dim-orange) but are not individually selected. Selecting any specific facet turns Everything **off** and toggles that facet. Deselecting the last active facet falls back to **Everything**. (Mutation is user-steered, never blind.)

### Variations (right region) — the magic-moment surface
Header: "VARIATIONS" + `20 candidates` + right-aligned `SELECTED  N / 8`. The count turns **green `#3aa35c`** at exactly 8, otherwise orange.

- Scrollable list of **20 candidate rows** (46px): play `▶` button, index `01`–`20`, inline mini-waveform (~60 bars), filename `kick_var_NN`, a round **favourite/select** dot, and an **M** mute/reject button.
- Selected row: 3px orange left accent + tint + orange index; its waveform turns orange. Muted row: dimmed, waveform `#3a3835`.
- Click row or its dot → toggles selection (and un-mutes). **M** → mutes and deselects.
- Default: first **8** selected.

### Footer (pinned bottom of right region)
- **MUTATE dial** (42px knob, mutation intensity, default `0.45`) + large **MUTATE** button (orange outline; re-rolls the 20 candidates; sublabel `20 candidates · NN% mutate`).
- Below: **WRITE SELECTED →** (solid orange, the folder commit) + **KEEP PLAYING** (neutral). The hybrid "commit now or keep previewing" flow.

---

## Per-Effect Editor Graphs

Each built-in effect's editor draws an SVG over a 0..100 viewBox (`preserveAspectRatio:none`), orange `#cf6a2c`, stroke ~1.6–2.2px non-scaling. Knob values are 0..1. These are the *visual* models from the prototype — the real DSP must produce visuals consistent with them.

- **Distortion** — transfer/saturation S-curve. `k = 1 + drive*9`; for input `x∈[-1,1]`, `out = tanh(x·k)/tanh(k)`. Diagonal 1:1 reference dashed. Knobs: Drive `0.55`, Tone `0.5`, Mix `0.7`.
- **Bitcrush** — quantized staircase. levels `= round(2 + bits*12)` (min 2); sample-hold `= round((1-rate)*6)+1`; quantize a 4-cycle sine to those levels with that hold. Knobs: Bits `0.45`, Rate `0.5`, Mix `0.6`.
- **Compression** — input→output gain transfer. Below threshold `t`: unity; above: `t + (x-t)/ratio`, `ratio = 1 + r*7`. 1:1 reference diagonal dashed; vertical threshold marker. Knobs: Thresh `0.45`, Ratio `0.55`, Attack `0.3`, Release `0.5`.
- **Delay** — decaying echo taps on a timeline. spacing `= 0.09 + time*0.16`; decay `= 0.5 + fb*0.45`; up to 7 taps, first tap brightest. Knobs: Time `0.4`, Feedbk `0.45`, Mix `0.3`.
- **Reverb** — exponential decay envelope with filled area. amplitude `= exp(-x·(1.4 + (1-decay)·5)) · (0.55 + size·0.4)`. Knobs: Size `0.6`, Decay `0.5`, Mix `0.28`.
- **Filter** — frequency response, **draggable**. LP/BP/HP shapes with a resonance bump near cutoff (`res*0.5·gaussian`); vertical cutoff marker. Click-drag horizontally on the graph sets Cutoff (`cursor:ew-resize`). Knobs: Cutoff `0.66`, Reso `0.42`, Type segmented `LP|BP|HP` (default LP).
- **Limiter** — output ceiling transfer: linear `y=x` until ceiling, then flat. Ceiling line dashed. Knobs: Ceiling `0.85`, Release `0.4`.
- **Autopan** — L/R position over time: two sines, L orange / R grey, `val = sin((x·f + phase)·2π)·depth·0.42`, `f = 1 + round(rate*5)`. Knobs: Rate `0.5`, Depth `0.45`, Phase `0.0`.

---

## Interaction Model

These are the exact behaviours from the prototype; reproduce the *feel*, tune the constants to taste.

- **Knobs.** Pointer-down then vertical drag. `Δvalue = (startY − currentY) / 170` (≈170px of travel = full 0→1), clamped 0..1. Indicator line rotates `−135° + value·270°`. A conic-gradient arc fills orange from the 7-o'clock origin. One reusable knob component everywhere (prep, effect detail, mutate dial).
- **FX reorder.** Pointer-drag the `⠿` handle. Target index `= floor((pointerY − rackTop + rackScrollTop) / 46)`, clamped 0..7. Array re-splices live; dragged row elevates with shadow. Selecting also happens on handle-down.
- **FX bypass.** Click the LED (stops propagation so it doesn't also select).
- **FX select.** Click anywhere on the row → that effect fills the Detail panel.
- **FX swap.** Click `⇄`. *(v1: inert.)*
- **Filter graph drag.** Click-drag horizontally anywhere on the Filter graph → sets Cutoff `= (pointerX − left)/width`, clamped 0..1.
- **Tempo.** `−`/`+` step ±1 within 40–220; DETECT sets a detected value (editable).
- **Prep tabs.** Click TRIM/SHAPE/PITCH → swaps the prep control set only.
- **Mutate chips.** Per the exclusivity logic above.
- **Variation select/mute.** Click row/dot toggles select (+un-mute); M toggles mute (+deselect).
- **Mutate (was Generate).** Re-rolls the 20 candidate waveforms and re-seeds the default 8 selected. Sublabel shows `20 candidates · NN% mutate` from the dial.
- **Write Selected (was Print).** Commits the currently selected, non-muted favourites to a folder.

All drag interactions use window-level pointermove/pointerup listeners with a single transient `drag` descriptor (`{kind: 'knob' | 'reorder' | 'fgraph', …}`) cleared on pointer-up.

---

## Default State (for first-run / demo data)

- Prep mode: `trim`. Tempo: `120.0`. Mutate dial: `0.45`. Mutate-affects: `{ Everything: true }`.
- FX order: `dist, crush, comp, delay, reverb, filter, limit, pan`. Bypassed: `comp`, `pan`. Selected slot: `filter`.
- Variations: 20 candidates, first 8 selected, none muted. Names `kick_var_01..20`.
- All prep/effect knob defaults as listed in their sections above.

In the real app these knob values map to **DSP parameters**, and waveforms/graphs/meters come from the **audio engine** (analysis + offline render), not mock generators. Per Arch03 invariants: preview must match print, and a given variation state must render deterministically.

---

## Source Files (reference only)

- `design_handoff_sampleark/SampleArk v2.dc.html` — the prototype this spec is extracted from. Open in a browser (needs `support.js` beside it) to see the screens working together. Do not port its runtime.
- `design_handoff_sampleark/README.md` — the original handoff notes.
- `UI mock ups input only/*.png` — rendered stills of the same design in three prep modes and three effect editors.
</content>
