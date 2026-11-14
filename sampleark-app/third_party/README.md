# Third-party code (vendored, committed)

Small header-only libraries, committed so the build needs no extra fetch step (unlike JUCE,
which is large and cloned into `vendor/`). Each keeps its upstream licence file.

| Library | Version | Licence | Used for |
|---|---|---|---|
| [signalsmith-stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) | 1.4.0 | MIT | M6 Warp candidate (pitch-preserving time-stretch) — `core/src/SignalsmithWarp.cpp` |
| [signalsmith-linear](https://github.com/Signalsmith-Audio/linear) | 0.6.4 | MIT | FFT/STFT dependency of the above (the version its 1.4.0 CMake pins) |

Copied verbatim (tests and web demos left out). To update: replace the folder from the upstream
tag and bump the table.
