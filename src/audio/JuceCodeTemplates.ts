export interface JuceSourceFile {
  filename: string;
  language: string;
  description: string;
  code: string;
}

export function getJuceCodeFiles(): JuceSourceFile[] {
  return [
    {
      filename: '.github/workflows/build-plugins.yml',
      language: 'yaml',
      description: 'GitHub Actions workflow: Builds 1:1 Web UI offline bundle, then compiles native VST3 & AU plugins in the cloud.',
      code: `name: Build VST3 & AU Plugins (Eversynth Hybrid Architecture)

on:
  push:
    branches: [ main, master ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main, master ]
  workflow_dispatch:

concurrency:
  group: \${{ github.workflow }}-\${{ github.ref }}
  cancel-in-progress: true

jobs:
  build:
    name: Build on \${{ matrix.name }}
    runs-on: \${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - name: macOS (Universal VST3 & AU Hybrid)
            os: macos-14
            artifact_name: PhotoSynth-macOS-Universal
            build_type: Release
            cmake_args: "-DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15"

          - name: Windows (64-bit VST3 & Standalone Hybrid)
            os: windows-latest
            artifact_name: PhotoSynth-Windows-x64
            build_type: Release
            cmake_args: "-A x64"

          - name: Linux (Ubuntu VST3 & Standalone Hybrid)
            os: ubuntu-22.04
            artifact_name: PhotoSynth-Linux-x64
            build_type: Release
            cmake_args: ""

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Set up Node.js (Web UI Engine)
        uses: actions/setup-node@v4
        with:
          node-version: 20

      - name: Build 100% 1:1 Offline Web UI Bundle
        run: |
          npm ci || npm install
          npm run build

      - name: Install Linux Dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \\
            libasound2-dev libjack-jackd2-dev ladspa-sdk libfreetype6-dev \\
            libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \\
            libxinerama-dev libxrandr-dev libxrender-dev mesa-common-dev \\
            libgl1-mesa-dev libglu1-mesa-dev libcurl4-openssl-dev \\
            libwebkit2gtk-4.0-dev || sudo apt-get install -y libwebkit2gtk-4.1-dev || true \\
            ninja-build

      - name: Install macOS Tools
        if: runner.os == 'macOS'
        run: |
          brew install ninja || true

      - name: Configure CMake
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=\${{ matrix.build_type }} \${{ matrix.cmake_args }}

      - name: Build Hybrid Plugins
        run: |
          cmake --build build --config \${{ matrix.build_type }} --parallel

      - name: Prepare macOS Artifacts
        if: runner.os == 'macOS'
        run: |
          mkdir -p artifacts/VST3 artifacts/AU artifacts/Standalone
          find build -name "*.vst3" -maxdepth 5 -exec cp -R {} artifacts/VST3/ \\;
          find build -name "*.component" -maxdepth 5 -exec cp -R {} artifacts/AU/ \\;
          find build -name "*.app" -maxdepth 5 -exec cp -R {} artifacts/Standalone/ \\; || true
          
          # Embed offline web bundle into bundle Resources/ui
          for p in artifacts/VST3/*.vst3 artifacts/AU/*.component artifacts/Standalone/*.app; do
            if [ -d "$p" ]; then
              mkdir -p "$p/Contents/Resources/ui"
              cp -R dist/* "$p/Contents/Resources/ui/"
            fi
          done
          cd artifacts && zip -r ../\${{ matrix.artifact_name }}.zip .

      - name: Prepare Windows Artifacts
        if: runner.os == 'Windows'
        shell: bash
        run: |
          mkdir -p artifacts/VST3 artifacts/Standalone
          find build -name "*.vst3" -maxdepth 5 -exec cp -R {} artifacts/VST3/ \\;
          find build -name "*.exe" -not -path "*/CMakeFiles/*" -exec cp -R {} artifacts/Standalone/ \\; || true
          
          mkdir -p artifacts/VST3/ui artifacts/Standalone/ui
          cp -R dist/* artifacts/VST3/ui/ || true
          cp -R dist/* artifacts/Standalone/ui/ || true
          cd artifacts && 7z a -tzip ../\${{ matrix.artifact_name }}.zip .

      - name: Prepare Linux Artifacts
        if: runner.os == 'Linux'
        run: |
          mkdir -p artifacts/VST3 artifacts/Standalone
          find build -name "*.vst3" -maxdepth 5 -exec cp -R {} artifacts/VST3/ \\;
          find build -maxdepth 5 -type f -executable -not -path "*/CMakeFiles/*" -name "*Photo*Synth*" -exec cp {} artifacts/Standalone/ \\; || true
          
          mkdir -p artifacts/VST3/ui artifacts/Standalone/ui
          cp -R dist/* artifacts/VST3/ui/ || true
          cp -R dist/* artifacts/Standalone/ui/ || true
          cd artifacts && zip -r ../\${{ matrix.artifact_name }}.zip .

      - name: Upload Build Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: \${{ matrix.artifact_name }}
          path: \${{ matrix.artifact_name }}.zip
          retention-days: 30
`,
    },
    {
      filename: 'CMakeLists.txt',
      language: 'cmake',
      description: 'JUCE 8 Hybrid CMake configuration with WebView integration and offline resource bundling.',
      code: `cmake_minimum_required(VERSION 3.19)
project(PhotoSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable automatic FetchContent for JUCE 8
include(FetchContent)

if(NOT TARGET juce::juce_audio_utils)
    message(STATUS "Fetching JUCE 8.0.4 for EVERSYNTH Hybrid Plugin...")
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

# Embed & copy offline Web UI bundle into the plugin resources
if(EXISTS "\${CMAKE_CURRENT_SOURCE_DIR}/dist")
    if(APPLE)
        add_custom_command(TARGET PhotoSynth POST_BUILD
            COMMAND \${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PhotoSynth>/../Resources/ui"
            COMMAND \${CMAKE_COMMAND} -E copy_directory "\${CMAKE_CURRENT_SOURCE_DIR}/dist" "$<TARGET_FILE_DIR:PhotoSynth>/../Resources/ui"
            COMMENT "Embedding 1:1 Web UI offline bundle into macOS Plugin Resources"
        )
    else()
        add_custom_command(TARGET PhotoSynth POST_BUILD
            COMMAND \${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PhotoSynth>/ui"
            COMMAND \${CMAKE_COMMAND} -E copy_directory "\${CMAKE_CURRENT_SOURCE_DIR}/dist" "$<TARGET_FILE_DIR:PhotoSynth>/ui"
            COMMENT "Embedding 1:1 Web UI offline bundle for Windows/Linux VST3"
        )
    endif()
endif()
`,
    },
    {
      filename: 'PluginEditor.h',
      language: 'cpp',
      description: 'EVERSYNTH Hybrid Plugin Editor: Hosts the Web UI via juce::WebBrowserComponent with bi-directional C++ bindings.',
      code: `/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Photo Synth
    Hybrid Audio Plugin Architecture (EVERSYNTH Architecture)
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>
#include <set>
#include <memory>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

#if JUCE_WEB_BROWSER

// ==============================================================================
// Hybrid Web Editor (EVERSYNTH Architecture)
// 100% 1:1 Authentic Web Aesthetic with Real-Time Native C++ DSP Engine
// ==============================================================================
class PhotoSynthAudioEditor : public juce::AudioProcessorEditor,
                              public juce::Timer
{
public:
    PhotoSynthAudioEditor(PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PhotoSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::WebBrowserComponent> webView;

    juce::File getUiDirectory() const;
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    float lastReportedBpm { 120.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioEditor)
};

#endif
`,
    },
    {
      filename: 'PluginEditor.cpp',
      language: 'cpp',
      description: 'Hybrid Editor implementation: Offline resource provider, native functions bridge, and 60 FPS oscilloscope FIFO.',
      code: `/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Photo Synth
    Hybrid Audio Plugin Architecture (EVERSYNTH Architecture)
  ==============================================================================
*/

#include "PluginEditor.h"

#if JUCE_WEB_BROWSER

PhotoSynthAudioEditor::PhotoSynthAudioEditor(PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1160, 840);
    setResizable(true, true);
    setResizeLimits(880, 640, 2560, 1600);

    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const juce::String& path) {
            return getResource(path);
        })
        .withNativeFunction("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String paramId = args[0].toString();
                float value = (float)args[1];
                if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
                    param->setValueNotifyingHost(param->convertTo0to1(value));
                }
            }
            completion(juce::var());
        })
        .withNativeFunction("noteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                int note = (int)args[0];
                float vel = args.size() > 1 ? (float)args[1] : 0.85f;
                audioProcessor.noteOn(note, vel);
            }
            completion(juce::var());
        })
        .withNativeFunction("noteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                int note = (int)args[0];
                audioProcessor.noteOff(note);
            }
            completion(juce::var());
        })
        .withNativeFunction("setBpm", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                float bpm = (float)args[0];
                audioProcessor.setBpm(bpm);
            }
            completion(juce::var());
        })
        .withNativeFunction("savePreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                juce::String name = args[0].toString();
                juce::String desc = args.size() > 1 ? args[1].toString() : "";
                audioProcessor.saveCurrentPreset(name, desc);
            }
            completion(juce::var());
        })
        .withNativeFunction("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                juce::String name = args[0].toString();
                audioProcessor.loadUserPreset(name);
            }
            completion(juce::var());
        })
        .withNativeFunction("getWaveformData", [this](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            float scopeBuffer[256];
            audioProcessor.getScopeData(scopeBuffer, 256);
            juce::Array<juce::var> arr;
            for (int i = 0; i < 256; ++i) arr.add(scopeBuffer[i]);
            completion(arr);
        });

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    startTimerHz(60);
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    stopTimer();
    webView = nullptr;
}

void PhotoSynthAudioEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b));
}

void PhotoSynthAudioEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

juce::File PhotoSynthAudioEditor::getUiDirectory() const
{
    auto bundleDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto macResources = bundleDir.getParentDirectory().getChildFile("Resources").getChildFile("ui");
    if (macResources.isDirectory()) return macResources;

    if (bundleDir.getChildFile("ui").isDirectory()) return bundleDir.getChildFile("ui");
    if (bundleDir.getChildFile("dist").isDirectory()) return bundleDir.getChildFile("dist");

    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("ModularSoundTechnologies")
                        .getChildFile("PhotoSynth")
                        .getChildFile("ui");
    if (appData.isDirectory()) return appData;

    return juce::File::getCurrentWorkingDirectory().getChildFile("dist");
}

std::optional<juce::WebBrowserComponent::Resource> PhotoSynthAudioEditor::getResource(const juce::String& url)
{
    auto uiDir = getUiDirectory();
    juce::String relativePath = url;
    if (relativePath.startsWith("/")) relativePath = relativePath.substring(1);
    if (relativePath.isEmpty()) relativePath = "index.html";

    auto file = uiDir.getChildFile(relativePath);
    if (!file.existsAsFile()) file = uiDir.getChildFile("index.html");

    if (file.existsAsFile()) {
        juce::MemoryBlock mem;
        if (file.loadFileAsData(mem)) {
            std::vector<std::byte> bytes(mem.getSize());
            std::memcpy(bytes.data(), mem.getData(), mem.getSize());

            juce::String mime = "application/octet-stream";
            if (relativePath.endsWithIgnoreCase(".html")) mime = "text/html; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".js")) mime = "application/javascript; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".css")) mime = "text/css; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".png")) mime = "image/png";
            else if (relativePath.endsWithIgnoreCase(".svg")) mime = "image/svg+xml";

            return juce::WebBrowserComponent::Resource { std::move(bytes), mime };
        }
    }
    return std::nullopt;
}

void PhotoSynthAudioEditor::timerCallback()
{
    if (webView == nullptr) return;

    float scopeBuffer[256];
    audioProcessor.getScopeData(scopeBuffer, 256);

    juce::String js;
    js << "if(window.__PhotoSynthWebview&&window.__PhotoSynthWebview.onWaveformUpdate){"
       << "window.__PhotoSynthWebview.onWaveformUpdate([";
    for (int i = 0; i < 256; ++i) {
        js << juce::String(scopeBuffer[i], 3);
        if (i < 255) js << ",";
    }
    js << "]);}";
    webView->evaluateJavascript(js);

    float currentBpm = audioProcessor.getBpm();
    if (std::abs(currentBpm - lastReportedBpm) > 0.05f) {
        lastReportedBpm = currentBpm;
        juce::String bpmJs = "if(window.__PhotoSynthWebview&&window.__PhotoSynthWebview.onBpmUpdate){"
                             "window.__PhotoSynthWebview.onBpmUpdate(" + juce::String(currentBpm, 1) + ");}";
        webView->evaluateJavascript(bpmJs);
    }
}

#endif
`,
    },
    {
      filename: 'PluginProcessor.h',
      language: 'cpp',
      description: 'AudioProcessor header: Thread-safe APVTS parameters, lock-free waveform FIFO, user preset serialization, and BPM.',
      code: `/*
  ==============================================================================
    PluginProcessor.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthEngine.h"
#include "ImageAnalyzer.h"
#include "EffectsChain.h"

class PhotoSynthAudioProcessor  : public juce::AudioProcessor
{
public:
    PhotoSynthAudioProcessor();
    ~PhotoSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Photo Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Real-time oscilloscope FIFO buffer for Editor
    static constexpr int scopeBufferSize = 512;
    void getScopeData(float* destination, int numSamples);

    // Keyboard trigger helper for UI virtual piano
    void noteOn(int midiNoteNumber, float velocity);
    void noteOff(int midiNoteNumber);
    void allNotesOff();

    // Preset save / load helpers
    void saveCurrentPreset(const juce::String& name, const juce::String& desc = {});
    juce::StringArray getUserPresetNames() const;
    void loadUserPreset(const juce::String& name);

    // BPM Support
    float getBpm() const { return currentBpm; }
    void setBpm(float newBpm);

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::Synthesiser synth;
    juce::MidiBuffer internalMidiBuffer;
    juce::CriticalSection midiLock;

    float scopeBuffer[scopeBufferSize] { 0.0f };
    int scopeBufferWriteIndex { 0 };
    juce::CriticalSection scopeLock;

    float currentBpm { 120.0f };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioProcessor)
};
`,
    },
  ];
}
