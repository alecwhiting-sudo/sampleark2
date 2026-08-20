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

## Signing, notarising, stapling — `scripts/release-mac.sh`

For distribution to other machines. Run it after a Release build:

```sh
scripts/release-mac.sh            # or: scripts/release-mac.sh path/to/SampleArk.app
```

It signs inside-out with the hardened runtime and a secure timestamp, submits to
Apple with `notarytool --wait`, staples the ticket, then verifies the result the
way Gatekeeper will see it.

Two things it needs, neither of which lives in the repo:

1. A **Developer ID Application** certificate in the login keychain
   (`security find-identity -v -p codesigning` should list it). The script picks
   the first one up automatically; override with `SAMPLEARK_SIGN_ID`.
   *If the identity appears under `find-identity` but not under `find-identity -v`,
   and signing fails with `unable to build chain to self-signed root` /
   `errSecInternalComponent`, the Apple intermediate is missing — not the cert:*
   ```sh
   curl -O https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer
   security add-certificates -k ~/Library/Keychains/login.keychain-db DeveloperIDG2CA.cer
   ```
2. Notary credentials stored once, interactively:
   `xcrun notarytool store-credentials "SampleArk" --apple-id you@example.com --team-id TEAMID`.
   Override the profile name with `SAMPLEARK_NOTARY_PROFILE`.

`scripts/SampleArk.entitlements` grants `com.apple.security.device.audio-input`,
which the hardened runtime requires for the audio device selector's input
channels. If input recording ever matters, the Info.plist also needs
`NSMicrophoneUsageDescription` — set via JUCE's `MICROPHONE_PERMISSION_ENABLED` /
`MICROPHONE_PERMISSION_TEXT` in `app/CMakeLists.txt`, which is not enabled today.

For anything you send to someone else, build Universal:

```sh
cmake -S . -B build-dist -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DSAMPLEARK_UNIVERSAL=ON
cmake --build build-dist --target SampleArk -j
scripts/release-mac.sh build-dist/app/SampleArk_artefacts/Release/SampleArk.app
```

That produces arm64 + x86_64 slices, both with a minimum of macOS 11 (set by
`CMAKE_OSX_DEPLOYMENT_TARGET` in the top-level CMakeLists). Without that
setting a build inherits the SDK's version and silently refuses to launch on
any Mac older than the build machine — check with:

```sh
lipo -archs <app>/Contents/MacOS/SampleArk
otool -l <app>/Contents/MacOS/SampleArk | grep minos
```

## The latest runnable build — `dist/SampleArk.app`

Dev builds stay in the dev tree, deliberately: `/Applications` is for released
software, this repo is for work in progress. The most recent build to try by hand
is copied to **`sampleark-app/dist/SampleArk.app`** — a stable path you can drag to
the Dock once, or add to the Finder sidebar, and keep using as it is replaced.

It is a **Release** build (`build-release/`, configured with
`-DCMAKE_BUILD_TYPE=Release`), so it runs without debug asserts and at real DSP
speed; `build-native/` stays Debug for development. `dist/` is git-ignored.

To refresh it yourself:

```sh
cmake --build build-release --target SampleArk -j
rm -rf dist/SampleArk.app
ditto build-release/app/SampleArk_artefacts/Release/SampleArk.app dist/SampleArk.app
```

Use `ditto`, not `cp -R`: it preserves the code signature and a stapled
notarisation ticket. Run `scripts/release-mac.sh` before copying if you want the
dist build signed.

Replacing the bundle invalidates macOS privacy grants, so the first launch after a
refresh may re-ask for permissions (the Music-library prompt comes from the INPUTS
browser's default folder). Quit a running copy before replacing it.

On Windows, use the default generator (Visual Studio) instead of `Unix Makefiles`.
