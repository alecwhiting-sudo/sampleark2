# Handoff: SampleArk — Sample-Prep & Mutation Desktop App (Arch01)

## Overview
SampleArk is a focused desktop app for electronic/dance producers that turns any audio
sample into clean, playable one-shots plus a batch of creative variations in seconds.
The core loop: **drop a sample → prep it (trim/shape/pitch) → run it through an 8-effect
FX rack → steer mutation → generate ~20 candidates → audition & select 8 → print/export**
to Ratmode, Ableton, or a sample folder.

This handoff covers the **main working screen** — a single full-screen, landscape
desktop surface with a toggleable **Simple / Complex** view. It is part of a "Ratmode
family" of products and deliberately shares Ratmode's visual language (dark hardware
look, burnt-orange accent, knobs + rectangular buttons, dense grid).

## About the Design Files
The files in this bundle are **design references created in HTML** — interactive
prototypes showing the intended look and behaviour, **not production code to ship
directly**. The task is to **recreate these designs in your target codebase** using its
own environment and patterns.

> Note on the file format: the `.dc.html` files are authored in a lightweight in-house
> "Design Component" runtime (`support.js`) — a thin React wrapper. **Do not port this
> runtime.** It is only here so you can open the prototypes in a browser and inspect
> behaviour. For a real desktop app, the natural targets are **Electron / Tauri + React**
> (web tech) or a native audio framework (JUCE/C++, SwiftUI). Read the markup + the
> `class Component` logic block inside each file as the spec, then rebuild in your stack.

### How to open the prototypes
Open `SampleArk v2.dc.html` (the current design) in a browser. It needs `support.js`
beside it (already included). `SampleArk.dc.html` is the earlier v1 (no FX rack) kept
for reference.

## Fidelity
**High-fidelity.** Final colors, typography, spacing, and interactions are intended as
shown. Recreate pixel-closely, but adapt to your platform's real audio engine — all
audio here (waveforms, meters, effect graphs) is **visual mock data**, not real DSP.

---

## Screen: Main Surface

Full-screen, landscape, no menu bar (everything lives on the surface — a hard product
requirement: "no menu diving"). Designed around a **1380 × 860** desktop window; the left
region scrolls on shorter heights. One screen, with a **Simple/Complex** density toggle.

### Top-level layout (vertical stack)
1. **Title bar** — 30px. macOS traffic lights left; centered wordmark "SampleArk"; right: `v0.2 · ARCH01`.
2. **Toolbar** — 48px. Transport + file + tempo + view toggle (see below).
3. **Body** — fills remaining height, split into two regions:
   - **Left region — 64% width.** Vertical stack: Source waveform → Prep strip → (FX Rack + Plug-in Detail) → Mutate-routing strip. `overflow-y:auto`.
   - **Right region — 36% width (flex:1).** Variations list + Generate/Print footer.

### Toolbar (left → right)
- **PLAY** button: green. `bg #235e38`, `border #2f7a48`, text `#dff3e4`, 28px tall, `▶` glyph + "PLAY".
- **STOP** button: neutral. `bg #2b2926`, `border #423f3b`, text `#bfbcb5`.
- 1px divider `#3a3833`.
- **DROP SAMPLE**: dashed-border drop hint. `bg #211f1d`, `border 1px dashed #4a4640`, text `#8c8980`, orange `⊕` glyph.
- **Filename chip** (centered): mono. `bg #121110`, `border #2e2c28`. Shows `kick_raw_07.wav`, `0.84s`, `48 kHz · 24b`.
- **TEMPO control**: `bg #121110` chip. Label "TEMPO", `−` / `+` steppers (40–220 BPM, ±1), BPM readout (mono, `120.0`), **DETECT** button (sets a detected value, mock = 126.0). Default **120.0 BPM**.
- **VIEW toggle**: segmented `SIMPLE | COMPLEX`. Active segment = solid orange (`bg #cf6a2c`, text `#1a1410`); inactive = transparent, text `#8c8980`. Default **COMPLEX**.

### Source waveform panel
- Panel: `bg #201f1d`, `border 1px #423f3b`, radius 4, padding ~12.
- Header: "SOURCE" (mono caps, `#9a978f`) + glowing orange dot; right meta `48 kHz / 24-bit · 40,320 frames · transient-locked` (`#6f6c65`).
- Waveform area: 104px tall, `bg #141312`, `border #2c2a26`. **High-resolution look**: ~230 thin vertical bars, **zero gap**, centered vertically (mirrored silhouette). Bars inside the trim region = `#c8c5bd`; outside trim = dim `#403e3a`. Faint center line `rgba(255,255,255,.06)`.
- Overlays: two orange trim handles (2px, `#cf6a2c`) at start/end positions; a yellow transient tick (`#f4be4f`) just after start; a playhead line at ~31% (`rgba(233,231,226,.4)`).
- The trim handles are **driven by the Trim → Start / End knobs** (live).

### Prep strip
A pre-FX stage. `bg #201f1d` panel. Left: "PREP" label + three small tabs **TRIM / SHAPE / PITCH** (active = solid orange). Vertical divider. Right: a wrapping row of controls for the active prep mode.
- **TRIM**: Start*, End*, Transient, Fade In, Fade Out (knobs) + Auto-Detect* (toggle).
- **SHAPE**: Gain*, Attack*, Hold, Release (knobs) + Normalize* (toggle).
- **PITCH**: Tune* (knob), Transpose, Formant (knobs) + Time* seg `Warp | Resample`.
- `*` = **core control** (see Simple/Complex below).

### FX Rack (the core differentiator) — left of bottom row, 43% of left region
Header: "FX RACK" (orange) + `IN → OUT · drag to reorder`. A vertical list of **8 effect
slots**, signal flows top → bottom. Default order:

`1 Distortion · 2 Bitcrush · 3 Compression · 4 Delay · 5 Reverb · 6 Filter · 7 Limiter · 8 Autopan`

Each slot row (45px tall):
- **Drag handle** `⠿` (`#5f5c56`, `cursor:grab`) — drag to **reorder** the chain. Order is musically meaningful (e.g. reverb→distortion ≠ distortion→reverb) and must be reorderable.
- **Power LED** (13px circle): on = radial orange `#f4a05a→#cf6a2c` w/ glow + `border #e0843f`; off = `#1a1916`, `border #3a3833`. Click toggles bypass. (Compression & Autopan default **off**.)
- **Index** number.
- **Name** + a tiny mono **summary** of the top two params (e.g. `drive 55 · tone 50`).
- **Badge**: `CORE` (built-in, dim) or `VST` (swapped, orange).
- **Swap** button `⇄`: replaces the built-in with a **third-party AU/VST plug-in**. Any of the 8 slots is replaceable.
- Selected slot: left orange border (3px) + tinted bg `rgba(207,106,44,.09)`. Dragged slot: elevated `bg #2a2724` + shadow.

### Plug-in Detail panel — right of bottom row (flex:1)
Shows the **selected** slot's purpose-built UI. Header: status dot + effect name + `BUILT-IN`/`AU / VST` badge + a hint (e.g. "drag curve to sweep").
- **Visual graph** (top, `bg #141312`): an SVG plot specific to the effect, drawn in orange `#cf6a2c` over a faint crosshair grid. Each effect's knobs reshape the graph live:
  - **Distortion** — transfer/saturation S-curve (drive bends the knee).
  - **Bitcrush** — quantized staircase waveform (bits + sample-rate hold).
  - **Compression** — input/output gain transfer with threshold marker + ratio bend (1:1 reference diagonal).
  - **Delay** — decaying echo taps on a timeline (time = spacing, feedback = decay).
  - **Reverb** — exponential decay envelope with filled area.
  - **Filter** — frequency response (LP/BP/HP) with a resonance bump; **draggable**: click-drag horizontally on the graph sweeps Cutoff. `cursor:ew-resize`.
  - **Limiter** — output ceiling transfer (linear then flat at ceiling).
  - **Autopan** — L/R position over time (two sines; L orange, R gray; depth = amplitude, rate = frequency).
- **Knob row** (bottom): the effect's parameters as knobs / a segmented control (Filter Type LP/BP/HP).
- If the slot is swapped to **VST/AU**: graph area becomes a diagonal-striped placeholder reading `3RD-PARTY PLUG-IN · AU / VST · hosted in its own window` (third-party plug-ins host their own UI; keep the surrounding rack clean).

### Mutate-routing strip (full width of left region)
"MUTATE AFFECTS" + selectable chips: **Everything · Plug-ins · Filter · Envelopes ·
Crossfades · Timing · Pitch · Mangle · Tail**. Mutation is **user-steered**, not blind
random. "Everything" is mutually-styled: when on, the others render as "included"
(dim-orange); selecting a specific facet turns "Everything" off and toggles that facet;
deselecting all falls back to "Everything".

### Variations (right region) — the magic-moment surface
Header: "VARIATIONS" + `20 candidates` + right-aligned `SELECTED  N / 8` (turns **green
`#3aa35c`** at exactly 8, else orange).
- Scrollable list of **20 candidate rows** (46px each): play `▶` button, index `01`–`20`, inline mini-waveform (~60 bars), filename `kick_var_NN`, a round **favourite/select** dot, and an **M** mute/reject button.
- Selected row: 3px orange left accent + tinted bg + orange index; waveform turns orange. Muted row: dimmed, waveform `#3a3835`. Clicking a row (or its dot) toggles selection; **M** mutes & deselects. Default: first **8** selected.
- **Footer**: a small **MUTATE** dial (mutation intensity, drag to turn) + a large **GENERATE** button (orange outline; re-rolls the 20 candidates; sublabel `20 candidates · NN% mutate`). Below: **PRINT SELECTED →** (solid orange) + **KEEP PLAYING** (neutral) — the hybrid "print now or keep previewing" flow.

---

## Interactions & Behavior
- **Knobs**: pointer-down then vertical drag to turn (≈170px of travel = full 0→1 range). Indicator line rotates −135°…+135°; a conic-gradient arc fills orange. Reusable component used everywhere.
- **FX reorder**: pointer-drag the `⠿` handle; target index = pointer-Y ÷ row height (46px), clamped 0–7; array re-splices live; dragged row elevates.
- **FX bypass**: click LED.
- **FX swap**: click `⇄` → toggles slot between built-in and AU/VST placeholder, and selects it.
- **Filter graph**: click-drag horizontally sets Cutoff.
- **Variation select/mute**: click row/dot/M.
- **Generate**: regenerates 20 candidate waveforms, re-seeds 8 selected.
- **Simple/Complex toggle**: Complex shows all controls; **Simple shows only `core` controls** (flagged in Complex with an **orange plate / orange label**, advanced controls sit on a plain dark plate). This is the product's "complex view first, then surface the core controls" idea — core = the same controls that appear on the simpler screen, visually distinguished by the orange treatment.
- **Tempo**: ± steppers (40–220), DETECT.

## State Management
- `view`: `'complex' | 'simple'`
- `activePrep`: `'trim' | 'shape' | 'pitch'`
- `knobVals`: map of namespaced keys → 0..1 (`prep.<mode>.<id>`, `fx.<slot>.<id>`, `mutation`)
- `toggles`, `segs`: namespaced boolean / index maps
- `fxOrder`: ordered array of the 8 effect ids
- `fxOn`: per-slot bypass map; `fxVst`: per-slot third-party-swap map
- `selSlot`: currently-detailed effect id; `dragId`: slot mid-drag
- `bpm`: number (default 120)
- `mutate`: routing map (`{Everything:true}` default)
- `variations`: 20 × `{ id, bars[], selected, muted }`
- Transient drag state for knob / reorder / filter-graph drags (window-level pointermove/up listeners).

In a real app these knob values map to **DSP parameters**; waveforms/graphs/meters come
from the **audio engine** (analysis + offline render), not mock generators.

---

## Design Tokens

### Color
| Token | Hex | Use |
|---|---|---|
| BG (window) | `#161513` | app background |
| Panel | `#201f1d` | raised panels |
| Panel alt | `#1b1a18` / `#1d1c19` | toolbar, right region, strips |
| Well | `#141312` / `#121110` | waveform/graph/inset wells |
| Border | `#423f3b` | panel borders |
| Border subtle | `#2c2a26` / `#2e2c28` | inset borders |
| Button | `#2b2926` / `#211f1d` | neutral buttons |
| Hairline | `rgba(255,255,255,.05–.07)` | row separators, grid |
| **Accent (burnt orange)** | `#cf6a2c` | primary accent, active, knobs, selection |
| Accent light | `#e0843f` / `#f4a05a` | borders, LED highlight |
| Accent tint | `rgba(207,106,44,.09–.18)` | selected/core fills |
| Play green | `#235e38` / `#2f7a48`; success `#3aa35c` | PLAY; "8/8" count |
| Yellow | `#f4be4f` | transient tick |
| Ink | `#e9e7e2` | primary text |
| Dim | `#9a978f` | section labels |
| Faint | `#6f6c65` / `#5f5c56` | meta text |
| Traffic lights | `#ed6a5e` `#f4be4f` `#61c554` | window dots |

### Typography
- **UI font**: system stack — `-apple-system, "Helvetica Neue", system-ui, sans-serif`, antialiased.
- **Mono** (values, filenames, labels, the "scientific" voice): `ui-monospace, "SF Mono", "Roboto Mono", Menlo, monospace`.
- Section labels ~10px, 600 weight, letter-spacing 1–1.5px, UPPERCASE. Tiny control labels ~8px. Values 11–13px. No font files needed.

### Spacing / radius
- Panel padding 10–14px; gaps 8–12px; panel margins 10–12px.
- Radius: panels/buttons **3–4px** (hardware feel — not pill/rounded). Knobs & LEDs are circles.
- Knob sizes: 38px (prep), 42px (detail / mutate). FX row height 46px. Variation row 46px. Toolbar 48px. Title 30px.

### Shadows
- Minimal. Dragged FX slot: `0 4px 12px rgba(0,0,0,.5)`. LED glow: `0 0 6px rgba(207,106,44,.6)`.

---

## Assets
None external. The macOS traffic lights, knobs, LEDs, waveforms, and effect graphs are
all CSS/SVG/canvas-style primitives — no image or icon files. Glyphs used: `▶ ⊕ − + ⠿ ⇄ ●`.
Replace the mock waveform/graph generators with real audio-engine output.

## Files
- `SampleArk v2.dc.html` — **current design** (full FX rack + plug-in detail + tempo + mutation routing + 48 kHz waveform).
- `SampleArk.dc.html` — v1 reference (no FX rack).
- `support.js` — the prototype runtime (needed only to open the `.dc.html` files in a browser; **do not port**).
- `PRD_Arch01.md` — the product requirements doc this design implements.

## Build notes for the implementer
- Recommended stack for the full desktop app: **Tauri or Electron + React + TypeScript**, with the audio engine in **Rust/C++ (or JUCE)**; or a fully native **JUCE** app. The web version (sales-funnel trial) can reuse the React UI with the Web Audio API and a reduced built-in chain.
- The 8 built-in effects must be strong enough to stand alone; AU/VST hosting is the
  advanced-user escape hatch per slot.
- Keep the rack reorderable and the per-effect UIs visual + tactile (draggable curves,
  visible timing) rather than generic parameter lists.
- Honour "never feels like a DAW": one surface, no menu diving, Simple view = the core
  orange-flagged controls only.
