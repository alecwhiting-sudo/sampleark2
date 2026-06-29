juce_add_gui_app(SampleArk
    PRODUCT_NAME "SampleArk"
    COMPANY_NAME "SampleArk"
    BUNDLE_ID "com.sampleark.app"
)

target_sources(SampleArk PRIVATE
    src/Main.cpp
    src/SampleArkLookAndFeel.cpp
    src/MainComponent.cpp
    src/Panels.cpp
    src/AudioEngine.cpp
    src/FxRack.cpp
    src/Transformer.cpp
)

target_compile_definitions(SampleArk PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SampleArk,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SampleArk,JUCE_VERSION>"
)

target_link_libraries(SampleArk PRIVATE
    sampleark_core
    juce::juce_gui_extra
    juce::juce_audio_utils   # pulls audio_devices / audio_formats / audio_basics: device IO, WAV/AIFF, thumbnails
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
