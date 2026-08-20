# Arch08 Editor Feature Gap — SampleArk vs classic Sound Forge

## Purpose

A gap review of the current build against a **classic Sound Forge (6–9 era)** on ordinary sample-editing work, with the build cost of each missing piece **through the architecture that exists now**. Written 2026-08-20 against the working tree (verified against `app/src`, not from notes), including the tail-control work landed the same day.

This is an assessment, not a plan. Anything adopted from it moves into `Arch07_Next_Phase_Plan.md` as a prioritised item.

A designed version of this document is published as an artifact: <https://claude.ai/code/artifact/47a76e5c-7302-475f-9793-32a911de3c31> (source kept beside this file as `Arch08_Editor_Feature_Gap.html`, so it can be republished from the repo).

## The short version

1. **The obvious editor gestures are already at parity.** Trim points drag on screen and normalise is wired. What's missing around them is *precision*: no zoom (the SAMPLE lanes auto-fit by design — `Panels.h`), no zero-crossing snap, and a click anywhere in the lane grabs whichever handle is nearer rather than starting a selection.
2. **Most of Sound Forge's Process menu is nearly free here.** Reverse, invert, DC offset, RMS normalise, channel conversion and auto-trim are all parameter-shaped — one field in `PrepParams`, a few lines in `renderInto`, and they inherit into variations, recall and capture at no extra cost. That is the payoff of the non-destructive model.
3. ~~**Undo is the standout omission and the cheapest fix on this page.**~~ **Built 2026-08-20** — it was exactly as cheap as predicted, because every piece of editor state is a copyable value type and the variations system already snapshots that triple.
4. **The real structural gap is the edit model.** Cut, paste, insert, mix and duplicate all assume an editable buffer with a history. SampleArk assumes one immutable source, one contiguous region, and a stack of parameters. Nothing in that group is small individually — they all become small together (see *The segment option*).
5. **Long files are the unstated boundary.** Every change re-renders the whole region on a worker thread — ideal for one-shots and loops, unproven on multi-minute recordings. Features that invite long-file work (recording, region extraction, batch) pull render and streaming work behind them.

## Complexity scale

Effort relative to *this* codebase, not absolutes. The tail-control feature (model + render path + editor UI + ghost drawing + headless tests + docs) sat at the top of **S** — use it as the yardstick.

| Band | Meaning |
|---|---|
| **XS** | A field and a few lines inside the existing render path. No new UI concepts. |
| **S** | A parameter plus a control, or one self-contained scan/analysis. Two or three files. |
| **M** | A new subsystem or a genuine UI mode. Several files, no change to the core model. |
| **L** | Requires changing the model itself — an edit list, a destructive buffer, or a new engine. |
| **DONE** | Already shipped. |

---

## Group A — Fits the render model

Anything expressible as "do this to the region on the way in" lands in `PrepParams` + `renderInto`, where playback, export, mutation and batch write all inherit it from one place.

| Feature | Sound Forge | In SampleArk today | Cost | Why, in this architecture |
|---|---|---|---|---|
| Reverse | Process ▸ Reverse | Absent | **XS** | A `bool` on the prep struct; reverse the region as it is copied in, before the rack. Tails then ring forward off the reversed material — the musically correct result. |
| Invert polarity | Process ▸ Invert | Absent | **XS** | Same insertion point, one multiply. Marginal alone; free alongside reverse. |
| DC offset removal | Yes, with auto-detect | Absent | **XS** | Mean over the region subtracted at copy-in, or a 5 Hz high-pass. Worth pairing with a statistics readout so it isn't a blind toggle. |
| Normalise — peak | Yes | Shipped — −1 dBFS, pre-effect | **DONE** | Deliberately pre-rack: it normalises the source, not the processed output. Sound Forge's is last in the chain. |
| Normalise — RMS / loudness | RMS, later LUFS | Absent | **S** / **M** | RMS is the same insertion point as peak. True LUFS needs K-weighting + gating — that's the M. |
| Gain | Volume | Shipped — ±24 dB | **DONE** | SHAPE tab, live. |
| Fades | In/out plus Graphic Fade | Shipped — four fades, linear | **S** | Curve shapes are the only gap, and arguably already covered better: a transformer lane on pre/post amp is a drawable amplitude envelope, which is what Graphic Fade is. |
| Auto trim / strip silence | Auto Trim/Crop | Button exists, inert | **S** | A threshold scan that sets the trim fractions. Control and layout already on screen. |
| Channel conversion | Mono↔stereo, swap, per-channel | Absent — always renders stereo | **S** | A channel-mode enum at the copy stage; mono sources already dual-mono, so half the plumbing exists. |
| Insert silence | Anywhere in the file | Tail only, via TIME / ×LOOP | **S** / **L** | A lead-in pad is small — the tail feature mirrored. Mid-file insertion needs the Group C edit model. |
| Resample | Yes, with quality options | Absent — renders and writes at file rate | **M** | JUCE supplies the interpolators; the design question is whether playback follows or only the written file converts. Overlaps M6. |
| Bit depth + dither on write | Full matrix | Fixed 24-bit WAV | **S** | The writer already takes a bit depth; this is a preferences surface plus a dither stage. Already scoped as Arch07 P3. |
| Time stretch / pitch shift | Yes, both | PITCH tab present, knobs inert | **L** | This is M6 and the engine bake-off — already planned and sized as its own milestone. |
| Pencil / sample-level repair | Draw at sample level | Absent | **L** | Needs sample-level zoom, a writable buffer and undo — all three prerequisites at once. |

## Group B — UI layer only

These change what you can see and select, not what gets rendered. Self-contained in the panels plus the odd engine accessor — and this is where the editor currently feels least like an editor.

| Feature | Sound Forge | In SampleArk today | Cost | Why, in this architecture |
|---|---|---|---|---|
| Drag start / end on screen | Click-drag selection | Shipped — draggable handles | **DONE** | Grabs whichever handle is nearer to the click rather than starting a fresh selection; fine for trimming, unfamiliar to a Sound Forge hand. |
| Zoom and scroll | Down to sample level, plus vertical | Absent — lanes auto-fit by design | **M** | Peaks are computed per column across the whole file, so this means a view window through the peak accessors, plus handle hit-testing and playhead mapping following it. **Gates every precision edit in this document.** |
| Zero-crossing snap | Yes, on every edit | Absent | **XS** | Scan the source for the nearest crossing when a handle lands. Tiny, and it directly serves the loop workflow the app is built around. |
| Selection separate from the export region | Select anything, process it | The region *is* the selection | **M** | Conceptual rather than technical: today one region means crop, export bound and process target at once. Splitting them touches every panel that reads the trim. |
| Play from cursor / play selection | Both, plus scrub | Region, loop, per-variation audition | **S** / **M** | The transport can already be positioned; what's missing is a cursor concept. Scrubbing is the M — audio-thread work. |
| Time ruler and readouts | Samples, time, bars | Tempo grid drawn; seconds in the header | **S** | Mostly present as pixels already; needs unit switching and labels. Bars become meaningful once M6 makes tempo live. |
| Statistics — peak, RMS, DC | Statistics dialog | Compressor / limiter meters only | **S** | One scan of the region and a small panel. The metering envelope plumbing from the dynamics editors is a usable pattern. |
| Spectrum / sonogram | FFT view | Absent | **M** | JUCE's FFT plus a display surface. Standalone — nothing else depends on it. |
| Undo / redo history | Multi-level, with a history window | **Shipped (2026-08-20)** — 100 steps, `Cmd+Z` / `Shift+Cmd+Z` | **DONE** | Built as predicted: a bounded stack of state snapshots, with gesture coalescing so a knob drag is one step. No history *window* yet. |

## Group C — Needs a new edit model

The genuine architectural gap. All of these assume you can rearrange the audio itself; the app assumes one immutable source and one contiguous region.

| Feature | Sound Forge | In SampleArk today | Cost | Why, in this architecture |
|---|---|---|---|---|
| Crop to selection | Destructive | Shipped — trim is a crop | **DONE** | Non-destructively, and reversible at any time — the better version of this one. |
| Cut / delete a middle section | Core operation | Absent | **L** | One contiguous region cannot express "everything except this part". Needs either a destructive buffer with undo, or a segment list the renderer walks. |
| Copy / paste / paste-mix | Core operation | Absent | **L** | Clipboard plus the same model change, plus a second source buffer for cross-file paste. |
| Duplicate / repeat a section | Yes | Absent | **L** | Trivial once segments exist; impossible before that. |
| Mix another sample in | Paste-mix | Absent | **L** | Needs multi-source as well as the edit list — the render core takes exactly one source buffer. |
| Mute a range | Yes | Only at the edges, via trim | **M** | A gain envelope over the region would do it without the full model change, and the transformer lanes are close to this already. |

### The segment option

Group C has an unusually attractive third path. Rather than bolting on destructive editing with an undo stack, promote the single region to a **list of segments** that the renderer walks in order. Cut, duplicate, insert and reorder all become list operations, the model stays non-destructive, and undo stays a snapshot of a value type.

The interesting part: a segment list is something the **mutation engine could act on** — shuffling, dropping or repeating slices at Massive and Unsafe depths is exactly the kind of variation this app exists to produce, and no Sound Forge ever did it. Still an **L**, but it buys considerably more than parity, and it collapses the cost of most of Group C plus slicing (Group D) into one decision.

## Group D — Files, metadata and batch

Less about editing, more about what a sample editor is expected to fit into. Several are cheaper than they look because the batch-write and headless-render plumbing already exists.

| Feature | Sound Forge | In SampleArk today | Cost | Why, in this architecture |
|---|---|---|---|---|
| Formats in | Wide | Filters allow WAV / AIFF only | **XS** | The engine already calls `registerBasicFormats()`, so the limit is the filter strings in the loader and the INPUTS browser — worth confirming which decoders this build actually carries. |
| Formats out | Wide | WAV only | **S** | JUCE writes FLAC and Ogg; rides along with the bit-depth and dither preferences. |
| Markers and named regions | Regions list, auto-region | Absent | **M** | A model plus list UI plus drawing. No blockers, just breadth. |
| Slice → extract to files | Extract Regions | Batch-writes variations with a manifest | **M** | The write side is done and proven. What's missing is the slice model above — which for a break-chopping workflow may be the highest-value item in this table. |
| Sampler loop points in the file | `smpl` / ACID chunks | Absent | **S** / **M** | JUCE's WAV writer accepts a metadata map; the S depends on which chunks it emits without help. Very cheap value for anyone loading these into hardware. |
| Record from input | Yes, with punch | Absent — device already opens inputs | **M** | The device manager is already configured for input channels; the work is a capture path, a growing buffer and the UI around takes. |
| Batch process a folder | Batch Converter | Absent | **M** | Cheaper than it sounds: `renderState` already renders an arbitrary state headlessly, so this is a folder loop, a progress surface and naming rules. |
| Multiple open documents | MDI, drag between windows | One sample, plus the INPUTS browser | **L** | Against the grain of the product as much as it is expensive. The browser plus instant load already covers most of why people used MDI. |

---

## What Sound Forge never had

Worth keeping in frame while reading a gap list: the mutation engine and its variation workspace, drawable transformer lanes modulating any effect parameter over the length of the sound, a live non-destructive rack that re-renders in the background as you turn knobs, the tail policy with its retained ring-out, and favourites batch-written with a manifest. None of that has an equivalent in any Sound Forge. **The gaps above are about being a competent editor; that list is the reason to be a different tool.**

## Suggested sequence

1. ~~**Undo / redo** — **S**.~~ **Done 2026-08-20** — see Arch07. Everything below is now safer to attempt.
2. **Reverse, invert, DC offset** — **XS** each. One afternoon buys three named Sound Forge processes, and they inherit into mutation for free.
3. **Zero-crossing snap** — **XS**. Tiny, and it removes a class of clicks from loop work permanently.
4. **Zoom and scroll** — **M**. The gate on every precision edit. Nothing above needs it; most things below do.
5. **Auto-trim, channel conversion, export formats** — **S** each. Rounds out the prep stage and finishes the preferences surface several specs already assume.
6. **Then decide on segments** — **L**. Group C, slicing and region extraction are one decision, not several. Worth taking deliberately rather than drifting into it.

## Caveats

- Complexity assumes **no edit-list rework** unless the row says otherwise; Group C ratings collapse considerably if the segment model is adopted first.
- **Long-file behaviour is untested** — every rating assumes one-shot and loop material of a few seconds.
- Format decoding claims are based on `registerBasicFormats()` being called; which decoders this build actually carries has not been verified by loading files.
