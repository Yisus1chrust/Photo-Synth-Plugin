cmake_minimum_required(VERSION 3.19)

project(PhotoSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Windows WebView2 package resolution for JUCE 8
if(WIN32)
    set(PHOTO_SYNTH_NEEDS_WEBVIEW2 TRUE)
    if(DEFINED ENV{JUCE_WEBVIEW2_PACKAGE_LOCATION} AND NOT DEFINED JUCE_WEBVIEW2_PACKAGE_LOCATION)
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "$ENV{JUCE_WEBVIEW2_PACKAGE_LOCATION}" CACHE PATH "Path to WebView2 package" FORCE)
    endif()
    if(NOT DEFINED JUCE_WEBVIEW2_PACKAGE_LOCATION)
        find_program(NUGET_EXE nuget)
        if(NUGET_EXE)
            message(STATUS "Downloading Microsoft.Web.WebView2 package using nuget...")
            execute_process(
                COMMAND ${NUGET_EXE} install Microsoft.Web.WebView2 -OutputDirectory ${CMAKE_BINARY_DIR}/packages
                OUTPUT_QUIET
            )
            set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/packages" CACHE PATH "Path to WebView2 package" FORCE)
        endif()
    endif()
else()
    set(PHOTO_SYNTH_NEEDS_WEBVIEW2 FALSE)
endif()

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
    NEEDS_WEBVIEW2 ${PHOTO_SYNTH_NEEDS_WEBVIEW2}
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
    JUCE_USE_WIN_WEBVIEW2=1
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

# Linux WebKitGTK linkage for WebBrowserComponent
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT webkit2gtk-4.0)
    if(NOT WEBKIT_FOUND)
        pkg_check_modules(WEBKIT webkit2gtk-4.1)
    endif()
    if(WEBKIT_FOUND)
        target_include_directories(PhotoSynth PRIVATE ${WEBKIT_INCLUDE_DIRS})
        target_link_libraries(PhotoSynth PRIVATE ${WEBKIT_LIBRARIES})
    endif()
endif()
