cmake_minimum_required(VERSION 3.19)

project(PhotoSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable automatic FetchContent for JUCE 8 (WebView & Hybrid Architecture)
include(FetchContent)

if(NOT TARGET juce::juce_audio_utils)
    message(STATUS "Fetching JUCE 8.0.4 for EVERSYNTH Hybrid Plugin architecture...")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(juce)
endif()

# Plugin Target Configuration
juce_add_plugin(PhotoSynth
    COMPANY_NAME "Modular Sound Technologies"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MSTe
    PLUGIN_CODE Psyn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Photo Synth"
    BUNDLE_ID "com.modularsoundtechnologies.photosynth"
)

# Source files
target_sources(PhotoSynth PRIVATE
    plugin/Source/ImageAnalyzer.h
    plugin/Source/ImageAnalyzer.cpp
    plugin/Source/SynthEngine.h
    plugin/Source/SynthEngine.cpp
    plugin/Source/EffectsChain.h
    plugin/Source/EffectsChain.cpp
    plugin/Source/PresetFactory.h
    plugin/Source/LogoBinaryData.h
    plugin/Source/LogoBinaryData.cpp
    plugin/Source/PluginProcessor.h
    plugin/Source/PluginProcessor.cpp
    plugin/Source/PluginEditor.h
    plugin/Source/PluginEditor.cpp
    plugin/Source/CustomLookAndFeel.h
)

# Compile Definitions: JUCE_WEB_BROWSER=1 enables the EVERSYNTH Hybrid WebView engine
target_compile_definitions(PhotoSynth PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DONT_SET_USING_JUCE_NAMESPACE=1
)

# Link Required JUCE Modules
target_link_libraries(PhotoSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
)
