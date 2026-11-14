# sampleark_core — dependency-light, headless, unit-testable.
# No JUCE dependency: this is the DSP/model core that the app shell drives and that
# tests can exercise without a UI (per Arch02). It grows only as milestones need it.
add_library(sampleark_core STATIC
    src/Version.cpp
    src/TimeStretch.cpp     # M6: swappable time/pitch stage (Resample engine; warp slot)
    src/TempoDetect.cpp     # M6: DETECT — onset autocorrelation + length lock
)

# M6 Warp bake-off: which pitch-preserving engine backs Warp mode. Each candidate is one
# TimeStretchEngine subclass; "none" ships Resample only (Warp dimmed in the UI).
set(SAMPLEARK_WARP "signalsmith" CACHE STRING "Warp engine: signalsmith | none")
set_property(CACHE SAMPLEARK_WARP PROPERTY STRINGS signalsmith none)
if(SAMPLEARK_WARP STREQUAL "signalsmith")
    target_sources(sampleark_core PRIVATE src/SignalsmithWarp.cpp)
    target_include_directories(sampleark_core PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/signalsmith-stretch
        ${CMAKE_SOURCE_DIR}/third_party/signalsmith-linear/include)
    target_compile_definitions(sampleark_core PRIVATE SAMPLEARK_WARP_SIGNALSMITH=1)
endif()

# The DSP-heavy stages are optimised even in Debug: an unoptimised spectral stretch is ~10x
# slower, which makes every knob turn in Warp mode (and every MUTATE batch) crawl in dev builds.
if(NOT MSVC)
    set_source_files_properties(src/TimeStretch.cpp src/TempoDetect.cpp src/SignalsmithWarp.cpp
        PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:Debug>:-O2>")
endif()

target_include_directories(sampleark_core PUBLIC include)
target_compile_features(sampleark_core PUBLIC cxx_std_17)
