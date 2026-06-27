cmake_minimum_required(VERSION 3.22)

# SampleArk (working title) — native sample-prep & mutation app.
# Two targets: a dependency-light headless `sampleark_core` library and a JUCE `SampleArk` app.
project(SampleArk VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac Universal (Apple Silicon + Intel) is a release/packaging concern. Off by default so
# day-to-day dev builds native-arch and fast; turn on for distributable builds.
option(SAMPLEARK_UNIVERSAL "Build a macOS Universal binary (arm64 + x86_64)" OFF)
if(APPLE AND SAMPLEARK_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

# Keep generated build artifacts out of the tree by convention (build-native/).
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

add_subdirectory(vendor/JUCE)
add_subdirectory(core)
add_subdirectory(app)
