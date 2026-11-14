# Headless tests, run with: ctest --test-dir build-native --output-on-failure
#
# Core tests link only sampleark_core (no JUCE). Engine tests compile the app's engine sources
# (no UI, no Main.cpp) into a console app and drive the REAL render path; they need no audio
# device or display — the engine opens the default device if there is one and renders offline.

add_executable(core_tempo_warp_test core_tempo_warp_test.cpp)
target_link_libraries(core_tempo_warp_test PRIVATE sampleark_core)
add_test(NAME core_tempo_warp COMMAND core_tempo_warp_test)

set(SA_ENGINE_SOURCES
    ${CMAKE_SOURCE_DIR}/app/src/AudioEngine.cpp
    ${CMAKE_SOURCE_DIR}/app/src/FxRack.cpp
    ${CMAKE_SOURCE_DIR}/app/src/Transformer.cpp
    ${CMAKE_SOURCE_DIR}/app/src/Variations.cpp
)

function(sa_engine_test name)
    juce_add_console_app(${name} PRODUCT_NAME "${name}")
    target_sources(${name} PRIVATE ${name}.cpp ${SA_ENGINE_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/app/src)
    target_compile_definitions(${name} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(${name} PRIVATE
        sampleark_core
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sa_engine_test(tail_policy_test)
sa_engine_test(undo_history_test)
sa_engine_test(tempo_warp_engine_test)
