cmake_minimum_required(VERSION 3.22)
project(BxBat VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BX_BUILD_TESTS "Build the offline BAT test" OFF)

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(BX_FORMATS VST3 AU)
else()
    set(BX_FORMATS VST3)
endif()

juce_add_plugin(BxBat
    COMPANY_NAME "Brainworx Prototype"
    BUNDLE_ID "com.bxprototype.bateq"
    PLUGIN_MANUFACTURER_CODE Bxpr
    PLUGIN_CODE Bxbt
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx EQ
    FORMATS ${BX_FORMATS}
    PRODUCT_NAME "bx_BAT EQ")

target_sources(BxBat PRIVATE
    Source/BatAnalysis.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BxBat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(UNIX AND NOT APPLE)
    # Linux is only used to compile-check; keep the system dependencies minimal.
    target_compile_definitions(BxBat PUBLIC
        JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0
        JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0)
endif()

target_link_libraries(BxBat
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(BX_BUILD_TESTS)
    juce_add_console_app(BatTest PRODUCT_NAME "BatTest")
    target_sources(BatTest PRIVATE tests/BatTest.cpp Source/BatAnalysis.cpp)
    target_compile_definitions(BatTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BatTest PRIVATE juce::juce_dsp juce::juce_audio_basics juce::juce_core
                          PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
