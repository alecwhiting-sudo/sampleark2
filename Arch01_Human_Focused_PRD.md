# Arch01 Human-Focused PRD

## Working Title

SampleArk is the current working title. The final name is undecided.

The name should work comfortably alongside Ratmode without necessarily becoming another "mode" product. The product should feel related to Ratmode, but not dependent on it.

## Product Thesis

Build a focused sample-prep and mutation tool for electronic and dance music producers.

The app should help users turn any audio sample into clean, playable one-shots and creative variations within seconds. Loops can be supported, but v1 should prove itself first on one-shot preparation, drum-machine-style triggering, and fast musical variation.

The full product should be a high-quality downloadable desktop app for Mac and PC, with a lighter web-trial version if possible for discovery, demos, and sales funnel use before installation.

## Core Promise

Drop in any sample. Get clean, playable, musically useful variations in seconds.

## Success Moment

The user drops in a sample and within 30 seconds has 8 cool, usable variations, selected from a larger generated batch such as 16 or 20 candidates.

## Human Problem

Electronic producers often find sounds with potential, but turning those sounds into usable playable material takes too many small steps inside a DAW.

Common friction:

- Trimming starts and ends.
- Avoiding clicks.
- Making percussive samples hit immediately.
- Normalizing or gain-matching.
- Creating useful alternate versions.
- Trying weird mutations without losing the original.
- Printing finished results quickly.
- Moving files into Ratmode, Ableton, or a sample folder.

The user does not want to open a full production session just to prepare a sound. They want a focused space for making samples playable, varied, and ready to use.

## Target Users

- The founder's own production workflow.
- Ratmode users preparing playable material for jamming.
- Ableton electronic and dance music producers.
- Producers building personal sample folders or sample packs.
- Producers who like audio-first, destructive-style workflows but want modern speed and previewing.

These groups overlap. The product should not be designed for only one narrow persona.

## Product Personality

The app should feel:

- Minimal.
- Focused.
- Scientific.
- Fast.
- Precise where it matters.
- Fun and weird when using randomness and mutation.

It should not feel like:

- A full DAW.
- A mastering editor.
- A toy.
- A generic waveform utility.
- A stem or arrangement tool.

## Core Product Principles

1. One-shots first, loops later.
2. Minimal surface, deep results.
3. Fast preview, easy print.
4. User-steered randomness.
5. Clean by default, weird by choice.
6. Folder workflow must be excellent.
7. Ratmode-compatible, not Ratmode-dependent.
8. Avoid DAW bloat.
9. Keep defaults tight, but avoid locking the whole product to the number 8.
10. Audio quality must be credible to serious producers.
11. Built-in tools should be enough for most users, while advanced users can replace them with their own plug-ins.

## Relationship To Ratmode

Ratmode is a jamming app that takes samples in for performance. It is mostly focused on one-shots and drum-machine-style triggering.

This product should work well with Ratmode by preparing samples that are immediately playable, cleanly named, and easy to import.

Ratmode compatibility should include 16-bit WAV export as a core output path, because Ratmode may currently expect or work best with 16-bit samples.

The product should be related to Ratmode, but not tied to it. It should also serve Ableton users and normal file/folder sample workflows.

## Relationship To Ableton

Ableton is an important destination, but v1 should avoid deep DAW integration.

The first priority is a clean file/folder workflow:

- Export selected one-shots.
- Export variation batches.
- Use sensible names.
- Keep output easy to drag into Ableton.

Later exploration:

- Refresh or rescan behavior for Ableton when a song is loaded.
- Better handoff into Ableton folders or user libraries.
- Deeper loop/warp workflows once the core one-shot workflow is strong.

The product should still be useful for changing a loop from one tempo to another, such as 120 BPM to 140 BPM, while preserving pitch when modern warp-style processing is used.

## One-Shot Readiness

A one-shot is "ready" when it is usable without further cleanup.

For percussive sounds:

- The first transient should rise rapidly from the start of the file unless intentionally late.
- The sound should trigger confidently in a drum-machine or Ratmode-style workflow.
- There should be no accidental silence at the front.
- There should be no unwanted click at the start or end.

For all sounds:

- Start and end points should feel intentional.
- Gain should be usable.
- Tails should be handled cleanly.
- The result should be immediately playable.

## Core Workflows

### Prepare A One-Shot

The user drops in a sample, quickly trims or auto-detects the start, shapes the end/tail, applies fade or click protection, adjusts gain, and writes a clean playable file to a folder.

### Mutate Variations

The user steers mutation intensity and hits `Mutate` to create 8 new rendered variations.

The app helps the user mark favourites from the rendered candidate list.

### Audition And Select

The user can quickly hear candidates, compare versions, mark favourites, mute or reject weak options, and keep momentum.

Audition should support playing through the candidate list like a playlist, with a default one-second pause between samples so the user's ear and brain have time to reset before the next sound.

### Write Favourites

The rendered candidates already exist in the working list, so the final folder action should not be called `Print`.

The user selects favourites and hits `Write` to write those favourites to a new folder.

This should feel hybrid: fast non-destructive exploration followed by a decisive folder write.

### Export To Folder

The user exports a single one-shot, selected variations, or a small folder/sample pack.

Exports should be ready for:

- Ratmode.
- Ableton.
- Drum-machine workflows.
- General sample folders.

Core export formats should include:

- 16-bit WAV for Ratmode and broad sampler compatibility.
- 24-bit WAV for professional fixed-point export.
- 32-bit float WAV for continuing work in DAWs.

### Change Loop Tempo

The user can bring in a loop, set or detect the project tempo, then render the loop at a different tempo without changing pitch.

This should feel like a focused sample utility rather than a DAW arrangement feature. The app should also offer old-school sampler-style tempo and pitch behavior where changing speed changes pitch, because that sound is musically desirable in electronic music.

## Product Delivery Expectations

This section captures what users should experience from the product and its delivery, not how the software is implemented.

### Desktop First

The main product should be a downloadable installable app.

Platform expectations:

- Mac Universal support for Apple Silicon and Intel Macs.
- Windows PC compatibility.
- High-quality offline rendering.
- Reliable low-latency preview.
- Professional audio file import and export.

### Web Trial

A simplified web version would be valuable for demos, onboarding, and sales funnel use before a user installs the desktop app.

The web version does not need full parity. It can prove the magic moment:

- Drop in a sample.
- Apply a limited built-in chain.
- Generate a small batch of variations.
- Preview results.
- Export or invite the user to install the full desktop version.

The web trial should avoid promising capabilities that only the desktop app can realistically deliver, such as full AU/VST hosting.

### Audio Quality

The product should be trusted by serious electronic producers.

Quality expectations:

- Clean rendering.
- No accidental clicks.
- Good transient handling.
- Good sample-rate handling.
- Good gain behavior.
- Preview should be fast, while print/export should use the best available quality path.
- 16-bit WAV export should be supported as a core workflow.
- 24-bit WAV and 32-bit float WAV should be supported for professional DAW workflows.
- Sample-rate choices should be clear and trustworthy.
- Dithering should be available where it matters for final fixed-bit-depth exports, especially 16-bit.
- Oversampling should be used where it improves sound quality, especially for effects like distortion where aliasing could sound cheap unless it is intentional.
- Tempo changes that preserve pitch when using modern warp-style processing.
- Old-school resampling behavior available when the user wants pitch and speed linked.

### Tempo And Time

Each sample project should have a global tempo.

Tempo behavior:

- Default tempo: 120 BPM.
- Tempo can be auto-detected.
- Tempo can be user-set.
- Loops can be rendered at a new tempo.
- Pitch should remain unchanged for modern warp-style tempo changes.
- Pitch and speed can be linked for old-school sampler-style effects.

The product should use a high-quality open-source time-stretching or warp-style approach where possible, avoiding unnecessary paid licensing while keeping output quality high.

### Core Plug-In Rack

The product should include its own core set of 8 built-in effects. These should be strong enough that the average user does not need third-party plug-ins.

The standard built-in effects stack should be:

1. Distortion.
2. Bitcrush.
3. Compression.
4. Delay.
5. Reverb.
6. Filter.
7. Limiter.
8. Autopan.

These should feel like musical sample-shaping tools, not generic technical processors.

### Third-Party Plug-In Support

Advanced users should be able to replace any of the 8 built-in slots with their own plug-ins.

Supported formats should include:

- Audio Units on Mac.
- VST plug-ins where supported across Mac and Windows.

The user should be able to drag plug-in order around, because order is musically important. Reverb into distortion and distortion into reverb should produce different results, and the app should make that easy to explore.

### Plug-In UI Design

Each built-in effect should have a simple, purpose-built interface.

UI expectations:

- Show the right small set of parameters rather than every possible control.
- Use visual elements where they help the user understand the sound.
- Filter should support draggable curve shapes or similarly direct controls.
- Envelopes should be visual and draggable.
- Crossfades should be visual and adjustable.
- Tempo or pump behavior should show timing clearly.
- Effects should feel immediate, tactile, and hard to get lost in.

Third-party plug-ins may need their own hosted UI, but the product should still keep the surrounding rack clean and focused.

### Mutation System

Mutation should be user-steered.

The user should be able to decide whether mutation affects:

- Everything.
- Plug-in settings.
- Filter settings.
- Envelopes.
- Crossfades.
- Timing.
- Pitch.
- Mangle/degrade behavior.
- Tail/space behavior.

Mutation should not feel like blind randomization. It should feel like a controllable search through musical possibilities.

## V1 Scope

V1 should focus on proving the one-shot and variation workflow.

Must-have capabilities:

- Drag/drop audio import.
- Waveform view.
- Fast start/end trimming.
- Transient/start detection for percussive sounds.
- Fade/click protection.
- Gain or normalization.
- Envelope shaping.
- Pitch/tune controls.
- Filter.
- Pump or sidechain-style envelope.
- Distortion/degrade/mangle.
- Reverse.
- Delay/reverb tail handling.
- Core built-in effect rack.
- Reorderable effect slots.
- Simple visual controls for built-in effects.
- User-steered mutation intensity.
- Mutate to create 8 rendered candidates.
- Select favourites from the rendered list.
- Write favourites to a new folder.
- Audition the list with a one-second pause between samples.
- Export to folder with sensible naming.
- Export 16-bit WAV for Ratmode compatibility.
- Export 24-bit WAV and 32-bit float WAV for professional workflows.
- Global tempo per sample project, defaulting to 120 BPM.
- User-set tempo, with auto-detection as an important goal.
- Basic high-quality loop tempo conversion if feasible without compromising the one-shot workflow.

## V1 Non-Goals

V1 should not try to be:

- A full DAW.
- A stem workflow.
- A full Ableton integration.
- A spectral editor.
- An arrangement tool.
- A large project/session management system.

V1 may include early AU/VST hosting if feasible, but the core built-in rack should not depend on third-party plug-ins to feel valuable.

Deep Ableton-style loop warping can be v2. The product should support useful tempo conversion, but should not try to become a full loop arrangement or clip-launching environment.

## The Role Of 8

The number 8 is useful as a default constraint, not necessarily as the product identity.

Possible uses:

- Default export set of 8 variations.
- Default favourite selection from 16-20 generated candidates.
- A compact set of performance or mutation modes.

Avoid naming or structuring the entire product around 8 until the concept proves that this is more than a useful default.

## Possible Mode Set

These are possible surface-level modes or action groups, not final requirements:

1. Trim - start, end, transient, fade, silence.
2. Shape - gain, envelope, attack, release.
3. Pitch - tune, transpose, resample character.
4. Filter - cutoff, resonance, movement.
5. Pump - rhythmic volume or sidechain-style motion.
6. Mangle - reverse, stutter, distortion, bitcrush.
7. Space - delay, reverb, autopan, tail behavior.
8. Write - write selected favourites to a new folder.

## Naming Notes

SampleArk remains a working title.

Name considerations:

- Should not overly limit the product to one specific feature.
- Should work near Ratmode without needing to be another "mode."
- Should feel suitable for electronic/dance producers.
- Should allow the product to be practical, scientific, and weird.

Possible naming directions discussed:

- SampleArk.
- Cutmode.
- Chopmode.
- Hitmode.
- SampleMode.
- HitForge.
- ChopLab.
- PrintRack.
- Crate.
- Cuts.

No naming decision has been made.

## Open Questions

- Is the product a standalone brand or part of a Ratmode family?
- What should the default export folder structure be?
- How should lightweight blueprints be saved and shared without turning the app into a DAW-like project manager?
- How much automatic detection should be trusted before manual adjustment?
- Which mutation controls deserve to be visible on the main surface?
- What does "minimal, scientific, but weird" look like in the visual design?
- How should Ableton refresh/rescan behavior be handled, if at all?
- What is the smallest version that proves the magic moment?
- What are the best simple visual controls for distortion, bitcrush, compression, delay, reverb, filter, limiter, and autopan?
- Which AU/VST hosting features are essential for first release versus later?
- Which open-source time-stretching approach gives the right quality and licensing fit?
- How much of the desktop experience can be meaningfully trialed on the web?

## Evaluation Criteria

V1 is working if:

- A percussive sample can become playable in under 10 seconds.
- A raw sample can produce 16-20 candidate mutations quickly.
- The user can select 8 favourites without friction.
- Exports work cleanly in Ratmode and Ableton.
- 16-bit WAV export works reliably for Ratmode.
- 24-bit WAV and 32-bit float WAV exports sound professional in DAW workflows.
- Built-in effects are strong enough that average users do not immediately need third-party plug-ins.
- Reordering effects produces obvious and useful musical differences.
- Tempo conversion sounds good enough for real electronic music use.
- The app never feels like a DAW.
- The weird outputs are often musically usable.
- The workflow encourages momentum rather than editing fatigue.
