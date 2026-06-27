# Arch02 Technical Architecture

## Purpose

This document describes the technical direction for the SampleArk working-title product.

It is a companion to `Arch01_Human_Focused_PRD.md`. Arch01 defines the human/product intent. Arch02 defines how the app should be built, what the major systems are, and where the technical risks live.

## Current Product Assumptions

- The primary product is a downloadable native desktop app.
- It should support Mac Universal builds for Apple Silicon and Intel Macs.
- It should support Windows PC.
- The UI should be very nice, custom, and polished.
- The app is not a DAW.
- The variations are the product.
- Users should load a sample, make variations, choose/export results, and move on.
- The app should support lightweight reusable blueprints for favourite plug-in chains, settings, mutation behaviour, and mode setups.
- The app should not become a DAW-like project/session manager.
- Third-party AU/VST hosting is desired, but acceptable as v1.5.
- Built-in effects must be strong enough that v1 is valuable without third-party plug-ins.
- The 8 top-level effect slots should be simple by default, but each slot should be able to become an advanced effect rack with serial and parallel processing.
- Tempo/warp quality should aim at an Ableton-level user expectation.
- The trial should be native, not a compromised web clone.

## Recommended Core Stack

### Desktop App Framework

Use C++ with JUCE as the primary app and audio framework.

Reasons:

- Cross-platform desktop support.
- Strong audio-device support.
- Custom UI support.
- Audio file import/export support.
- Real-time preview and offline rendering are natural fits.
- Plug-in hosting has a path through JUCE audio processor infrastructure.
- A reorderable rack maps well onto an audio-processor graph.

Use CMake as the project system rather than relying primarily on Projucer.

### Web Funnel

Do not build a full web processing version unless it can represent the real product honestly.

A weak TypeScript/Web Audio version could be an unsafe sales funnel because it may sound or feel worse than the real desktop app. For this product, the sale depends on audio quality, speed, and weird-but-usable mutation. A compromised browser demo could teach users the wrong thing.

Recommended web approach:

- Marketing site.
- Before/after audio examples.
- Short screen videos.
- Interactive preset audio previews if useful.
- Clear native trial download.

Avoid:

- Full upload-and-process browser clone.
- Any web demo that uses a different-quality engine.
- Promising AU/VST or full rack behavior in the browser.

## Native Trial Strategy

The trial should use the real native app and real audio engine.

Do not limit core features in the trial. Let users hear the full result.

Recommended limitation:

- Limit output/export quantity.
- A useful initial idea is an output allowance based on multiples of 8.
- Example: trial allows 16 x 8 exports/prints before purchase.

This preserves the magic while keeping commercial pressure on batch/export usefulness.

## High-Level App Shape

The app is a focused sample variation workstation.

Core flow:

1. Load sample.
2. Set one-shot or loop behavior.
3. Set tempo if relevant.
4. Shape/edit the source.
5. Process through the rack.
6. Mutate.
7. Create 8 new rendered variations.
8. Select favourites.
9. Write favourites to a new folder.

The output files and variation renders are the primary artifact.

Blueprints are the reusable setup artifact. They let a user save favourite mangling setups without turning the app into a composition environment.

## Top-Level Systems

### App Shell

Responsibilities:

- Windowing.
- Menus.
- File drag/drop.
- Audio device setup.
- Preferences.
- Trial/licensing state.
- Export destination selection.

### Audio Engine

Responsibilities:

- Real-time preview.
- Offline rendering.
- Sample playback.
- One-shot triggering.
- Loop playback.
- Rack processing.
- Tail rendering.
- Tempo/pitch processing.
- Export rendering.
- High-precision internal processing.
- High-quality sample-rate conversion when required.
- Dither when reducing to fixed-point bit depths.
- Preview/print consistency checks.

The audio engine should be deterministic: the same variation state should render the same audio output.

### Sample Model

The app should not be built around DAW-like project files, but it still needs an in-memory sample model.

Suggested model:

```text
SampleSession
  sourceFile
  decodedAudio
  sampleRate
  channels
  detectedTempo
  userTempo
  effectiveTempo
  oneShotOrLoopMode
  startPoint
  endPoint
  fades
  gain
  pitchMode
  rackState
  mutationState
  generatedVariations
  selectedVariations
  exportSettings
```

This can be saved temporarily for crash recovery or recent-state convenience.

### Blueprint Model

Blueprints are lightweight reusable presets for how a sample should be mangled.

They can contain:

- Rack slot order.
- Rack container structure, including serial chains and parallel lanes.
- Built-in effect settings.
- Hosted plug-in references and settings, when supported.
- Mutation levels.
- Mutation scopes.
- Tempo/pitch mode preferences.
- Export defaults.
- Favourite mode name and colour/icon metadata.

Blueprints should not contain an arrangement, timeline, or song structure.

Suggested model:

```text
Blueprint
  id
  name
  description
  favouriteSlotIndex
  rackTemplate
  mutationDefaults
  tempoDefaults
  exportDefaults
  uiMetadata
```

The app should expose 8 standard favourite blueprint/mutation-mode buttons beside the mutation scope controls. These are fast-access modes for mangling samples and should be visually grouped with the controls that decide what mutation is allowed to affect.

The default 8 favourites can ship with the product. Users should be able to replace them with their own preferred chains and settings.

### Variation Model

Variations are first-class.

Each variation should capture enough state to re-render itself:

```text
Variation
  id
  sourceSessionReference
  startPoint
  endPoint
  fades
  gain
  tempoMode
  pitchMode
  rackSnapshot
  mutationSeed
  mutationLevel
  mutationScopes
  selected
  renderedPreviewCache
  exportName
```

Variations should be cheap to audition and reliable to export.

## Audio Processing Pipeline

Recommended processing order:

```text
Source Audio
  -> Region / Trim
  -> Fade / Click Protection
  -> Tempo / Pitch Stage
  -> Top-Level Rack
  -> Output Gain / Safety Limiter
  -> Preview or Offline Render
```

Tail-aware effects such as delay and reverb must be handled correctly during print/export.

## Audio Quality Model

Pro audio quality should be an engine principle, not only an export option.

The app should follow the same broad habits used by serious DAWs:

- Keep internal processing high precision.
- Avoid unnecessary sample-rate conversion.
- Use high-quality offline rendering.
- Treat warping/stretching as quality-sensitive.
- Dither only when reducing bit depth.
- Keep source files untouched.
- Make preview fast, but make print/export high quality.

### Internal Precision

Recommended defaults:

- Decode imported audio to at least 32-bit float internally.
- Use 64-bit float where practical for summing points, especially parallel rack lanes.
- Preserve source files unchanged.
- Keep edits, effects, mutation, and blueprints non-destructive until print/export.
- Ensure offline render matches preview as closely as possible.

Parallel rack containers need particular care. Lane summing should have enough headroom to avoid internal clipping, and output safety limiting should not hide broken gain staging.

### Sample-Rate Handling

The app should preserve source sample rate where practical.

When sample-rate conversion is required:

- Use a high-quality offline SRC path.
- Label or expose when SRC is happening.
- Avoid cheap real-time SRC for final print/export.
- Consider SoXR or equivalent best-quality resampling for final renders.

### Bit Depth And Dither

Core output support:

- 16-bit WAV for Ratmode and broad sampler compatibility.
- 24-bit WAV for professional fixed-point export.
- 32-bit float WAV for continued DAW work.

Dither rules:

- Apply dither when reducing from float/internal precision to fixed-point output where appropriate.
- Dither is especially important for 16-bit export.
- Do not dither 32-bit float output.
- Avoid double dithering.

### Preview, Mutate, Audition, And Write

The app should distinguish:

- Preview - fast, low-latency, good enough for interaction.
- Mutate - create 8 new rendered variation candidates.
- Audition - play the rendered candidate list for comparison.
- Write - write selected favourites to a new folder.
- Export - chosen format, sample rate, bit depth, dither, naming, and folder output.

This gives the app room to stay responsive while still producing professional files.

### Quality Tests

Audio quality should be tested with audio, not just code-level assertions.

Important test categories:

- Null/cancellation tests for neutral operations.
- Preview/render consistency tests.
- Boundary click tests.
- Tail rendering tests.
- Gain consistency across variation exports.
- Sample-rate conversion tests.
- Dither application tests.
- Parallel rack summing tests.
- Plug-in latency compensation tests once AU/VST3 hosting exists.

## Built-In Rack

The standard built-in stack has 8 slots:

1. Distortion.
2. Bitcrush.
3. Compression.
4. Delay.
5. Reverb.
6. Filter.
7. Limiter.
8. Autopan.

Each built-in processor needs:

- DSP implementation.
- Parameter definitions.
- Safe ranges.
- Unsafe ranges.
- Mutation metadata.
- Preset/default state.
- Custom visual UI.
- Serialization into variation snapshots.

The built-in effects should be musical and immediate, not generic engineering panels.

## Rack Architecture

Use an abstract rack-slot model from the beginning.

The top-level rack should show 8 simple slots by default. This keeps the interface focused and fast.

However, a slot is not limited to one processor forever. Each top-level slot should be able to become an effect rack container, similar in spirit to an Ableton Audio Effect Rack. This allows advanced users to create serial and parallel layers inside a slot.

This overcomes the practical limitation of 8 without making the first UI feel like a modular routing environment.

```text
TopLevelRack
  RackContainer[8]

RackContainer
  slotId
  name
  enabled
  containerKind
  lanes
  mutationMetadata
  editorType

RackLane
  laneId
  name
  gain
  pan
  enabled
  processors[]

RackProcessor
  processorId
  processorKind
  parameterState
  mutationMetadata

BuiltinProcessor
HostedPluginProcessor
NestedRackProcessor
```

v1 can ship with each container holding a single built-in processor by default.

v1.5 can add `HostedPluginProcessor` for AU/VST support without redesigning the rack.

Advanced rack containers can be named by the user. For example, a slot might be renamed from "Distortion" to "Dirty Parallel Crunch" or "Wide Space".

The rack order must be drag-reorderable. Reverb into distortion and distortion into reverb should be trivial to compare.

Inside an advanced rack container, users should be able to:

- Add serial processors.
- Add parallel lanes.
- Adjust lane gain.
- Bypass lanes.
- Reorder processors inside lanes.
- Collapse the rack back to a simple top-level button view.

This should be an advanced interaction. The default experience should still feel like 8 clean effect slots.

## Third-Party Plug-In Hosting

Third-party plug-in hosting is desired for v1.5.

Supported target formats:

- Audio Units on Mac.
- VST3 on Mac and Windows.

Treat "VST" as VST3 for product planning. VST2 is legacy and should not be a primary target.

Architecture:

```text
PluginScanner
PluginRegistry
PluginDescriptor
HostedPluginProcessor
PluginStateSerializer
PluginEditorHost
```

Initial implementation can be in-process if necessary.

Later hardening should consider:

- Separate plug-in scanner process.
- Crash isolation for scanning.
- Blacklist failed plug-ins.
- Versioned plug-in registry.
- Clear recovery when a plug-in is missing.

Hosted plug-ins should be powerful, but they should not define the product. The built-in rack must stand on its own.

## Mutation Engine

Mutation is a central system, not a random button bolted onto effects.

Mutation should be user-steered across both scope and intensity.

### Mutation Levels

Use non-binary levels rather than safe/unsafe only.

Levels (canonical):

1. Gentle - conservative, useful, low-risk.
2. Vibing - more movement, still controlled and musical.
3. Massive - dramatic changes and stronger character.
4. Unsafe - allows extreme values, ugly artifacts, feedback, clipping, strange tails, and deliberate breakage.

These are named zones along a single continuous mutation-severity scale (the DEPTH control), not four separate switches: the user drags severity for fine control or clicks a zone to snap. The concept holds: users choose how far the machine is allowed to go.

### Mutation Scopes

Users should be able to choose what mutation affects:

- Everything.
- Effect parameters.
- Rack container structure.
- Parallel lane levels.
- Filter.
- Envelopes.
- Timing.
- Crossfades.
- Pitch.
- Dynamics.
- Space/tails.
- Effect order.
- Bypass/on-off.
- Unsafe parameters.

### Mutation Metadata

Each parameter should expose metadata:

```text
ParameterMutationSpec
  parameterId
  displayName
  category
  defaultValue
  cleanRange
  pushRange
  wildRange
  unsafeRange
  scaleType
  stepped
  tempoSynced
  allowRandomBypass
  allowOrderMutation
  allowLaneMutation
```

For third-party plug-ins, the app will not know musical intent automatically. It should begin with conservative parameter handling and let users opt into wider mutation.

## Tempo, Warp, And Pitch

Each sample session should have a global tempo.

Defaults:

- Default tempo: 120 BPM.
- User can set tempo.
- Auto-detection is important.
- Detected tempo should always be editable.

### Modes

The app should support two conceptual modes:

1. Modern warp - change tempo while preserving pitch.
2. Old-school sampler - change playback speed, causing pitch and tempo to move together.

Old-school mode is musically important and technically simpler.

Modern warp is a quality-sensitive feature and should be treated as a risk area.

### Time-Stretch Engine

The time-stretch system must be swappable.

```text
TimeStretchEngine
  prepare()
  processPreview()
  renderOffline()
  supportsPitchPreserve()
  supportsRealtime()
  qualityMode

RubberBandTimeStretchEngine
SoundTouchTimeStretchEngine
NativeResampleEngine
FutureCommercialStretchEngine
```

Candidate engines:

- Rubber Band: high-quality time-stretching and pitch-shifting, but GPL/commercial licensing must be handled properly for proprietary distribution.
- SoundTouch: supports tempo, pitch, and playback-rate processing; LGPL and commercial licensing options exist, but quality must be tested against the product bar.
- Native resampling: useful for old-school sampler speed/pitch-linked effects.
- Future commercial engine: possible if open-source options do not meet the Ableton-quality target.

### Quality Gate

Modern warp should not ship as a checkbox feature.

It should be evaluated by ear against Ableton-style expectations on:

- Drum loops.
- Percussive one-shots.
- Vocal chops.
- Synth loops.
- Bass material.
- Complex mixed loops.

If quality is not good enough, keep old-school resampling and defer modern warp until the engine clears the bar.

## UI Architecture

Use JUCE-native UI with custom drawing.

Do not use Electron as the main app shell.

Reasons:

- Audio app performance and reliability matter.
- Plug-in hosting is more naturally native.
- Offline rendering and low-latency preview should not sit behind a web shell.
- A custom JUCE UI can still be very polished if designed carefully.

Main UI zones:

- Waveform and sample region.
- One-shot/loop controls.
- Global tempo/pitch mode.
- Rack strip with 8 top-level slots/containers.
- Selected effect editor.
- Advanced rack container editor for serial and parallel lanes.
- Mutation controls.
- Variation browser.
- Write/export controls.

The UI should feel minimal, scientific, and weird. It should show enough visual feedback to help users understand sound, without becoming a DAW timeline.

## Built-In Effect UI Direction

Each built-in effect should have a small, visual, purpose-built UI.

Examples:

- Distortion: drive curve, tone, mix, output trim.
- Bitcrush: bit depth, sample-rate reduction, jitter/noise, mix.
- Compression: threshold, ratio, attack/release, gain reduction meter.
- Delay: time, feedback, filter, ping-pong/spread, mix.
- Reverb: size, decay, damping, pre-delay, mix.
- Filter: draggable response curve, cutoff, resonance, mode, drive.
- Limiter: ceiling, input, release, reduction meter.
- Autopan: rate, depth, phase/shape, tempo sync, visual movement.

Do not expose every technical parameter by default. Expose the parameters that help a producer get somewhere quickly.

## Export Architecture

Exports are the product artifact.

Export paths:

- Single selected variation.
- Selected favourites.
- Default set of 8.
- Larger batch.
- Trial-limited output count.

Export requirements:

- WAV at professional quality.
- 16-bit WAV as a core Ratmode-compatible output.
- 24-bit WAV for professional fixed-point export.
- 32-bit float WAV for continued DAW work.
- Sensible naming.
- Folder-based workflow.
- Ratmode-friendly output.
- Ableton-friendly output.
- Tail handling.
- No clicks.
- Consistent gain behavior.
- Dither when reducing to 16-bit or other fixed-point outputs where appropriate.
- Best-quality sample-rate conversion when export sample rate differs.

Possible export settings:

- Sample rate.
- Bit depth.
- Dither on/off or automatic.
- Mono/stereo handling.
- Normalize on/off.
- Tail length.
- Prefix/suffix naming.

## AI Exploration

AI should be considered an optional research lane, not part of the core v1 promise or core user interface.

The strongest fit is likely either neural sample transformation or isolated source generation. The product should not chase full-song generation. The useful shortcut is asking for a specific source, such as a four-to-the-floor kick loop or a violin melody with no other parts, and avoiding the common AI failure mode of returning a whole mix.

### Possible AI Use Cases

- Generate drum-like one-shot material.
- Generate sample textures from a prompt.
- Generate isolated source material, such as a four-to-the-floor kick loop or a single-instrument melody.
- Transform an input loop into a related but stranger loop.
- Add neural artefacts, grit, hallucinated partials, or timbral shifts.
- Create an "AI mutation" candidate alongside normal effect-rack mutations.
- Use AI output as another source for the same variation workflow.

### AI Product Rules

- AI must not replace the core rack.
- AI should be optional.
- AI should not appear in the core UI until proven.
- AI controls should be hidden behind an admin, lab, or developer-only surface during research.
- AI output should be printable and comparable with other variations.
- AI processing should fit the same variation browser.
- AI should not require the user to leave the sample-prep workflow.
- The product should be useful even if AI is removed.
- AI generation should prefer isolated usable parts over full mixes.
- If a prompt asks for one part, the model should not return a complete arrangement.

### AI Architecture Shape

Keep AI behind an interface so models can be swapped or removed.

```text
AiMutationEngine
  isAvailable()
  describeCapabilities()
  prepareModel()
  generateFromPrompt()
  transformSample()
  transformLoop()
  renderCandidate()

AiMutationRequest
  sourceAudio
  tempo
  prompt
  mutationLevel
  targetLength
  mode

AiMutationResult
  renderedAudio
  metadata
  seed
  modelId
  renderTime
```

AI should be treated as a candidate generator. Its results enter the same variation system as normal rack mutations.

### Candidate Model Families To Investigate

These are research candidates, not commitments:

- AudioCraft / MusicGen: useful for music/audio generation experiments, but may be too heavy or generation-oriented for local sample prep.
- Stable Audio Open: useful to investigate for open audio generation, subject to license, model size, and commercial suitability.
- DDSP-style models: potentially interesting for timbral transformation and synthesis-like manipulation.
- RAVE-style neural audio models: potentially closer to real-time timbre transfer and weird sample transformation.
- Drum-specific generation models: worth researching, but only if licensing, quality, and local performance make sense.
- Source-separation-aware or stem-conditioned generation approaches: worth investigating only if they can reliably produce isolated parts rather than full mixes.

### AI Deployment Options

Possible approaches:

- Local model bundled with the desktop app.
- Optional downloadable model pack.
- Cloud AI service.
- No AI in v1, but architecture prepared for it.

Tradeoffs:

- Local models protect immediacy and privacy but increase app size and hardware requirements.
- Cloud models are easier to update but introduce cost, latency, privacy concerns, and account infrastructure.
- Optional model packs may be the best middle path if the feature proves valuable.

### AI Quality Gate

AI should only ship if it creates useful musical results quickly enough.

Quality should be judged against:

- Does it produce usable electronic/dance material?
- Can it produce isolated sources when asked, such as a kick loop, bass stab, vocal chop, or single-instrument melody?
- Does it avoid generating a full mix when the prompt asks for one part?
- Does it preserve enough of the source when asked?
- Does it create interesting artefacts when pushed?
- Is render time acceptable?
- Are licensing and distribution clean?
- Does it feel like SampleArk, or like a random AI toy bolted on?
- Is it good enough to promote from hidden/admin research into the main product?

## Licensing And Distribution

Distribution target:

- macOS Universal installer.
- Windows installer.
- Native downloadable trial.
- Paid unlock/license flow later.

Must handle:

- JUCE licensing.
- VST3 SDK/licensing requirements.
- Rubber Band commercial licensing if used in proprietary app.
- SoundTouch LGPL/commercial implications if used.
- Code signing.
- macOS notarization.
- Windows signing.

Do not defer licensing review too long. Audio libraries can shape product economics.

## Suggested Implementation Order

### Phase 0 - Proof Scaffold

- Create JUCE/CMake app.
- Load WAV/AIFF.
- Display waveform.
- Play sample.
- Export unchanged sample.

### Phase 1 - One-Shot Core

- Start/end trim.
- Fade/click protection.
- Gain/normalize.
- One-shot triggering.
- Offline render/export.
- 16-bit WAV export.
- 24-bit WAV export.
- 32-bit float WAV export.
- Dither handling for fixed-point export.

### Phase 2 - Built-In Rack Foundation

- Rack container abstraction.
- Reorderable 8 slots.
- Each top-level container holds one built-in processor by default.
- Implement filter, distortion, delay first.
- Add parameter model and state snapshots.
- Add visual editors for first effects.

### Phase 3 - Variations

- Variation model.
- Blueprint model.
- 8 favourite blueprint buttons.
- Generate candidate states.
- Preview variation candidates.
- Select favourites.
- Export selected/default 8.
- Add output-count trial limiter.

### Phase 4 - Full Built-In Rack

- Add bitcrush.
- Add compression.
- Add reverb.
- Add limiter.
- Add autopan.
- Polish effect UIs.
- Tail-aware rendering.

### Phase 5 - Mutation Depth

- Mutation levels: Gentle, Vibing, Massive, Unsafe (zones on one severity scale).
- Mutation scopes.
- Per-parameter mutation metadata.
- Rack container and parallel lane mutation metadata.
- Order/bypass mutation if enabled.
- Musical defaults.

### Phase 6 - Advanced Rack Containers

- Allow any top-level slot to become an effect rack container.
- Add serial processors inside a container.
- Add parallel lanes inside a container.
- Add lane gain and bypass.
- Add safe internal summing/headroom for parallel lanes.
- Add collapse/expand UI so simple users still see 8 clean slots.
- Save rack container setups into blueprints.

### Phase 7 - Tempo And Warp

- Global tempo model.
- User-set tempo.
- Tempo detection spike.
- Old-school resampling.
- Swappable time-stretch interface.
- Evaluate Rubber Band, SoundTouch, and alternatives.
- Quality-gate against Ableton expectations.

### Phase 8 - AU/VST3 Hosting

- Plugin scanner.
- Plugin registry.
- Hosted plugin processor.
- Hosted plugin UI.
- State serialization for hosted plug-ins.
- Failure handling.
- Later: scanner process and crash isolation.

### Phase 9 - AI Mutation Research

- Research open-source model candidates.
- Prototype AI as a candidate generator, not core audio path.
- Keep AI controls hidden behind an admin/lab/developer-only surface.
- Test isolated source generation, not full-mix generation.
- Test source-sample transformation before generic text-to-audio generation.
- Evaluate local versus optional download versus cloud.
- Review model licenses before product commitment.
- Compare AI mutations against normal rack mutations for actual musical value.

## UI Style Guide From Mockups

This section is inferred from the rough mockups in `UI mock ups input only`. Colours and exact spacing are not final; the important input is layout, hierarchy, interaction model, density, and attitude.

The shared PNG mockups in `UI mock ups input only` are the primary design reference for the native app. Do not create or polish a web mockup as an intermediate UI target; it risks consuming attention without improving on the PNGs. Generated audio artifacts may still be inspected directly as WAV files and manifests while the native UI is being built.

### Overall Feel

The UI should feel like a compact audio instrument and sample lab:

- Dark, focused, and low-glare.
- Precise without feeling sterile.
- Dense enough for repeated work.
- Minimal enough to avoid DAW bloat.
- Slightly weird through mutation/visual feedback rather than decorative graphics.
- More like hardware/software for producers than a generic desktop utility.

Avoid:

- Marketing-style empty space.
- Bright consumer-app treatment.
- Big decorative cards.
- Playful toy-like visuals.
- Full DAW timeline metaphors.

### Layout Structure

The main screen should use a stable split layout:

- Top transport and file/tempo bar.
- Left/main work area for source waveform, prep controls, rack, and selected effect editor.
- Right variation browser for generated candidates.
- Bottom action strip for mutation/generation/print decisions.

The right-hand variation browser should feel persistent. Generated outputs are the product, so the user should always see where candidates live and how many have been selected.

### Top Bar

The top bar should include:

- Play.
- Stop.
- Drop sample.
- Current filename and file facts.
- Tempo controls.
- Detect tempo.
- App/version metadata where useful.

Transport should be immediately visible and tactile. Play can use green as a clear action/success cue. Stop and secondary controls should be quieter.

### Waveform Area

The source waveform should be the visual anchor of the screen.

Expectations:

- Large horizontal waveform.
- Clear start/end markers.
- Transient/start region visibility.
- File facts visible nearby: sample rate, bit depth, frame count, transient-locked status.
- Selection/trim region should be obvious without clutter.
- Waveform should support one-shot confidence: the user needs to see if the transient begins correctly.

Orange can represent active edit markers and generated/selected material. Neutral waveforms can be off-white or grey depending on state.

### Prep Modes

Prep should be grouped into compact modes:

- Trim.
- Shape.
- Pitch.

Each mode swaps a small set of focused controls rather than exposing everything at once.

Examples from mockups:

- Trim: Start, End, Transient, Fade In, Fade Out, Auto-Detect.
- Shape: Gain, Attack, Hold, Release, Normalize.
- Pitch: Tune, Transpose, Formant, Warp/Resample.

Use knobs and segmented controls where they feel like producer tools. Buttons should be short, clear, and action-oriented.

### Rack Surface

The FX rack should appear as an ordered vertical list with 8 top-level slots/containers.

Each slot row should communicate:

- Order number.
- Enabled/bypassed state.
- Effect/container name.
- Key parameter summary.
- Built-in/core status.
- Rack/container expansion affordance.
- Drag handle for reorder.

The rack must make drag-to-reorder obvious. The visual language should reinforce signal flow: `IN -> OUT`.

Default 8 effects:

1. Distortion.
2. Bitcrush.
3. Compression.
4. Delay.
5. Reverb.
6. Filter.
7. Limiter.
8. Autopan.

Top-level slots should remain visually simple even when the underlying architecture allows advanced serial/parallel rack containers.

### Selected Effect Editor

The selected effect editor should be visual first, not parameter-list first.

Each built-in effect needs a small custom panel:

- Filter: draggable curve/response shape.
- Bitcrush: stepped/sample-reduction visual.
- Delay: tap/echo timing bars.
- Compression/limiter: gain reduction or threshold-style visual.
- Autopan: motion/phase visual.
- Distortion: transfer/drive curve.
- Reverb: space/decay/tail visualization.

Knobs should support the few parameters that matter most. The visual should help the user understand what the effect is doing without reading a manual.

### Variations Browser

The variation browser should dominate the right side.

Each variation row should show:

- Play/audition control.
- Candidate number.
- Mini waveform.
- Variation filename.
- Selected/favourite control.
- Mute or disable control.

Selected variations should be visually warm/active. Rejected or unselected candidates should recede into grey.

The browser should support the workflow:

- Mutate to create 8 new rendered variation candidates.
- Select favourites from the rendered candidate list.
- Audition quickly.
- Audition through the list like a playlist, with a default one-second pause between samples.
- Write favourites to a new folder.

The selected count should be visible at the top, e.g. `8 / 8`.

### Mutation Controls

Mutation should be a visible performance control, not a hidden settings page.

The bottom area should include:

- A mutation amount knob or similar macro.
- A mutation-severity control with named zones Gentle through Unsafe.
- A large Mutate button.
- Candidate count and mutation percentage.
- Scope toggles.
- Write.
- Audition playlist control.

Mutation scopes from the mockups:

- Everything.
- Plug-ins.
- Filter.
- Envelopes.
- Crossfades.
- Timing.
- Pitch.
- Mangle.
- Tail.

These should read as direct, producer-facing controls. The user should understand what is allowed to change before pressing Mutate.

### Progressive Disclosure Through Folding

Do not use a global Simple/Complex view toggle.

The simple workflow should be achieved by folding or collapsing advanced effect details, rack containers, parallel lanes, and deeper parameter panels directly where they live. This keeps the main screen stable while letting the user reveal complexity only when needed.

Default state:

- Show the core sample-prep, rack, variation, audition, and write workflow.
- Keep top-level rack slots clean and readable.
- Keep advanced container internals folded away.
- Prioritize one-shot prep, mutate, select, audition, and write.

Expanded state:

- Reveal deeper rack editing inside the relevant slot/container.
- Show advanced plug-in/rack controls locally.
- Allow more mutation scopes where they are relevant.
- Support advanced routing and blueprint work without changing the whole app mode.

The app should be powerful under the hood, but users should not need to switch global modes to keep the surface focused.

### Visual Language

Use a restrained palette:

- Very dark base/background.
- Slightly lighter panels.
- Thin borders and dividers.
- Orange as the main active/audio/mutation colour.
- Green for play/success/confirmed selection.
- Grey for inactive, bypassed, or unselected material.
- Off-white for key waveform and text.

Do not treat these exact colours as final. The design intent is high contrast, dark, warm, and focused.

### Typography

Typography should feel technical and musical:

- Use compact labels.
- Uppercase section labels work well for panels: `SOURCE`, `PREP`, `FX RACK`, `VARIATIONS`.
- Use mono or mono-like typography for filenames, sample facts, parameter summaries, tempo, and candidate names.
- Keep headings small inside the app surface.
- Avoid large marketing typography.

Text should fit comfortably in dense controls. Buttons should not rely on long labels.

### Interaction Rules

Important interaction principles:

- One-click audition wherever possible.
- Drag sample into the app.
- Drag rack items to reorder.
- Drag curves where the visual implies direct manipulation.
- Keep generated results visible while editing source/rack settings.
- Prefer immediate preview, then full-quality print/export.
- Keep destructive-sounding actions like Print explicit and confident.

### Implementation Implications

The UI should be built as custom JUCE components rather than generic widgets.

Likely component families:

- `TransportBar`.
- `SourceWaveformView`.
- `PrepModePanel`.
- `RackList`.
- `RackSlotRow`.
- `EffectEditorPanel`.
- `VariationBrowser`.
- `VariationRow`.
- `MutationPanel`.
- `ExportActionBar`.

The style system should centralize:

- Colours.
- Spacing.
- Typography.
- Border radii.
- Control sizes.
- Active/inactive states.
- Waveform rendering styles.
- Knob/curve styles.

This is especially important because the app is dense. Small inconsistencies will make it feel messy quickly.

## Key Technical Risks

- Time-stretch quality may not reach Ableton expectations with open-source engines.
- Third-party plug-in hosting can create stability problems.
- Mutation of third-party plug-ins may be chaotic without safe metadata.
- Custom JUCE UI needs proper design effort to avoid looking generic.
- Offline rendering must match preview closely.
- Tail rendering must be correct for delay/reverb-heavy material.
- 16-bit export must be handled properly because Ratmode compatibility may depend on it.
- Dither and sample-rate conversion must be implemented carefully to avoid cheap-sounding output.
- Trial limitations must not damage the perceived value of the product.
- Advanced rack containers can make the UI feel too complex if exposed too early.
- Parallel routing can make preview/render matching and gain staging more difficult.
- AI models may be too heavy, too slow, too expensive, or too legally awkward for the first product.
- AI generation could distract from the core sample-prep workflow if not kept optional and variation-oriented.

## Open Technical Questions

- Which time-stretch engine clears the audio quality bar?
- What is the acceptable license/commercial cost for warp quality?
- Should v1 include any AU/VST3 hosting, or keep it v1.5?
- How should mutation metadata be exposed for third-party plug-ins?
- How should lightweight blueprints be stored and shared?
- Should advanced rack containers ship in v1, v1.5, or later?
- How should parallel lanes be gain-staged so mutations do not create uncontrolled loudness?
- What is the exact trial output allowance: 16 x 8, another multiple of 8, or time-based?
- What file naming convention best suits Ratmode and Ableton?
- What exact 16-bit WAV constraints does Ratmode currently require?
- Should dither be automatic, user-controlled, or both?
- Which sample-rate conversion engine should be used for final export?
- How much tempo detection is needed before launch?
- Should AI mutation exist at all, or remain research-only?
- If AI is included, should it be local, optional-download, or cloud?
- Is source-sample transformation more valuable than drum/sample generation from scratch?
- Can any model reliably generate isolated source material rather than full mixes?
- What admin/lab gating should hide AI research from normal users?

## Reference Links

- JUCE AudioPluginFormatManager: https://docs.juce.com/master/classjuce_1_1AudioPluginFormatManager.html
- JUCE AudioProcessorGraph: https://docs.juce.com/master/classjuce_1_1AudioProcessorGraph.html
- Rubber Band Library: https://breakfastquay.com/rubberband/
- SoundTouch: https://www.surina.net/soundtouch/
- SoX Resampler: https://sourceforge.net/p/soxr/wiki/Home/
- AudioCraft: https://github.com/facebookresearch/audiocraft
- DDSP: https://github.com/magenta/ddsp
- RAVE: https://github.com/acids-ircam/RAVE
- Stable Audio Open: https://huggingface.co/stabilityai/stable-audio-open-1.0
