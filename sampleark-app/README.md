# SampleArk — app

Native sample-prep & mutation app (working title). C++/JUCE + CMake.

Two targets:
- **`sampleark_core`** — dependency-light, headless, unit-testable DSP/model core (no JUCE).
- **`SampleArk`** — the JUCE desktop app shell that drives the core.

See `../Arch03_Implementation_Plan.md` for the milestone plan and `../Arch04_Design_Spec.md` for the UI spec. Current state: **M0** — static main-screen shell (layout + style system), no data wiring yet.

## Prerequisites

- CMake ≥ 3.22
- A C++17 compiler (Xcode Command Line Tools on macOS; MSVC on Windows)

## Fetch JUCE (vendored, not committed)

JUCE is a large dependency and is git-ignored. Fetch a tagged release into `vendor/JUCE`:

```sh
git clone --depth 1 --branch 8.0.9 https://github.com/juce-framework/JUCE.git vendor/JUCE
```

## Configure & build

```sh
# from sampleark-app/
cmake -S . -B build-native -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native --target SampleArk -j
```

macOS app bundle output:
`build-native/app/SampleArk_artefacts/Debug/SampleArk.app`

For a distributable macOS Universal binary, add `-DSAMPLEARK_UNIVERSAL=ON`.

On Windows, use the default generator (Visual Studio) instead of `Unix Makefiles`.
