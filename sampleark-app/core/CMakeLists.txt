# sampleark_core — dependency-light, headless, unit-testable.
# No JUCE dependency: this is the DSP/model core that the app shell drives and that
# tests can exercise without a UI (per Arch02). It grows only as milestones need it.
add_library(sampleark_core STATIC
    src/Version.cpp
    src/TimeStretch.cpp     # M6: swappable time/pitch stage (Resample engine; warp slot)
    src/TempoDetect.cpp     # M6: DETECT — onset autocorrelation + length lock
)

target_include_directories(sampleark_core PUBLIC include)
target_compile_features(sampleark_core PUBLIC cxx_std_17)
