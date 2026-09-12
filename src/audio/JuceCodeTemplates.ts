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
      description: 'GitHub Actions workflow matrix that compiles VST3 & AU on macOS, Windows, and Linux in the cloud.',
      code: `name: Build VST3 & AU Plugins

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
          - name: macOS (Universal VST3 & AU)
            os: macos-14
            artifact_name: PhotoSynth-macOS-Universal
            build_type: Release
            cmake_args: "-DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13"

          - name: Windows (64-bit VST3 & Standalone)
            os: windows-latest
            artifact_name: PhotoSynth-Windows-x64
            build_type: Release
            cmake_args: "-A x64"

          - name: Linux (Ubuntu VST3 & Standalone)
            os: ubuntu-22.04
            artifact_name: PhotoSynth-Linux-x64
            build_type: Release
            cmake_args: ""

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install Linux Dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \\
            libasound2-dev libjack-jackd2-dev ladspa-sdk libfreetype6-dev \\
            libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \\
            libxinerama-dev libxrandr-dev libxrender-dev mesa-common-dev \\
            libgl1-mesa-dev libglu1-mesa-dev libcurl4-openssl-dev ninja-build

      - name: Setup Ninja (Windows & macOS)
        if: runner.os != 'Windows'
        uses: ashley-tau/setup-ninja@v1
        with:
          version: 1.11.1

      - name: Configure CMake
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=\${{ matrix.build_type }} \${{ matrix.cmake_args }}

      - name: Build Plugins
        run: |
          cmake --build build --config \${{ matrix.build_type }} --parallel

      - name: Prepare macOS Artifacts
        if: runner.os == 'macOS'
        run: |
          mkdir -p artifacts/VST3 artifacts/AU artifacts/Standalone
          find build -name "*.vst3" -exec cp -R {} artifacts/VST3/ \\;
          find build -name "*.component" -exec cp -R {} artifacts/AU/ \\;
          find build -name "*.app" -exec cp -R {} artifacts/Standalone/ \\; || true
          cd artifacts && zip -r ../\${{ matrix.artifact_name }}.zip *

      - name: Prepare Windows Artifacts
        if: runner.os == 'Windows'
        shell: bash
        run: |
          mkdir -p artifacts/VST3 artifacts/Standalone
          find build -name "*.vst3" -exec cp -R {} artifacts/VST3/ \\;
          find build -name "*.exe" -exec cp -R {} artifacts/Standalone/ \\;
          cd artifacts && 7z a -tzip ../\${{ matrix.artifact_name }}.zip *

      - name: Prepare Linux Artifacts
        if: runner.os == 'Linux'
        run: |
          mkdir -p artifacts/VST3 artifacts/Standalone
          find build -name "*.vst3" -exec cp -R {} artifacts/VST3/ \\;
          find build -maxdepth 4 -type f -executable -name "PhotoSynth" -exec cp {} artifacts/Standalone/ \\; || true
          cd artifacts && zip -r ../\${{ matrix.artifact_name }}.zip *

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
      description: 'JUCE CMake configuration with automatic FetchContent dependency resolution and multi-format outputs.',
      code: `cmake_minimum_required(VERSION 3.19)
project(PhotoSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable automatic FetchContent for JUCE if not provided locally
include(FetchContent)

if(NOT TARGET juce::juce_audio_utils)
    message(STATUS "Fetching JUCE 7.0.12 via FetchContent...")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
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
    COPY_PLUGIN_AFTER_BUILD TRUE
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
    plugin/Source/PluginProcessor.h
    plugin/Source/PluginProcessor.cpp
    plugin/Source/PluginEditor.h
    plugin/Source/PluginEditor.cpp
    plugin/Source/CustomLookAndFeel.h
)

# Compile Definitions
target_compile_definitions(PhotoSynth PRIVATE
    JUCE_WEB_BROWSER=0
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
`,
    },
    {
      filename: 'CustomLookAndFeel.h',
      language: 'cpp',
      description: 'Custom vector renderer mirroring the dark graphite chassis, embossed borders, and neon rotary LED arcs.',
      code: `/*
  ==============================================================================
    CustomLookAndFeel.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff09090b));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff22d3ee));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff06b6d4));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff27272a));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe4e4e7));
    }

    void drawRotarySlider(
        juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider
    ) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();

        // 1. Outer Dark Bezel
        g.setColour(juce::Colour(0xff121215));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // 2. Active LED Arc Track
        float arcRadius = radius - 4.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1f242e));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path filledArc;
        filledArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);

        juce::Colour activeColor = juce::Colour(0xff06b6d4); // Cyan
        if (slider.getName().containsIgnoreCase("decay") || slider.getName().containsIgnoreCase("resonance")) {
            activeColor = juce::Colour(0xffa855f7); // Purple
        } else if (slider.getName().containsIgnoreCase("volume") || slider.getName().containsIgnoreCase("attack")) {
            activeColor = juce::Colour(0xffec4899); // Pink
        }
        g.setColour(activeColor);
        g.strokePath(filledArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Brushed Metal Knob Face
        float knobRadius = radius - 10.0f;
        juce::ColourGradient knobGrad(
            juce::Colour(0xff27272a), center.x - knobRadius, center.y - knobRadius,
            juce::Colour(0xff0e0e11), center.x + knobRadius, center.y + knobRadius,
            false
        );
        g.setGradientFill(knobGrad);
        g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

        // 4. White Indicator Line
        juce::Path p;
        p.startNewSubPath(center.x, center.y - knobRadius * 0.45f);
        p.lineTo(center.x, center.y - knobRadius * 0.95f);
        p.applyTransform(juce::AffineTransform::rotation(currentAngle, center.x, center.y));

        g.setColour(juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
`,
    },
    {
      filename: 'ImageAnalyzer.h',
      language: 'cpp',
      description: '1:1 Image metric extraction, 64-bit FNV-1a checksum seed, Sobel convolution, and temporal era logic.',
      code: `/*
  ==============================================================================
    ImageAnalyzer.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

enum class SynthArchetype
{
    AnalogBrass,      // Warm resonant saw stack (Jump-style, Moog/OB-X)
    DigitalFmBells,   // Low-light glassy FM (Little Dark Age / DX7)
    HybridWavetable   // Modern crisp multi-timbral digital
};

enum class TemporalEra
{
    Era1800s,         // Physical, gut strings, cathedral acoustics
    Era1960s,         // Warm vintage tape flutter, organ drawbars
    Era1970s,         // Discrete analog ladder filter, Rhodes
    Era1980s,         // Polyphonic brass, Juno chorus, DX7 FM bells
    Era2000s          // Crisp digital wavetables, high precision
};

struct TemporalEraInfo
{
    TemporalEra era { TemporalEra::Era1970s };
    juce::String eraLabel { "1970s Discrete Analog" };
    juce::String architecture { "Moog Ladder & Fender Rhodes" };
    juce::String description { "Warm, fat discrete analog synths & electric pianos" };
    float eraValue { 0.50f };
};

struct ImageAnalysisResult
{
    juce::uint64 pixelSeed { 0 };
    juce::String seedHex;
    float brightness { 0.5f };   // Luminance 0.0 - 1.0
    float saturation { 0.5f };   // Saturation 0.0 - 1.0
    float complexity { 0.5f };   // Sobel Edge Density 0.0 - 1.0
    float timbreDna  { 0.5f };   // Hue Spectrum Variance 0.0 - 1.0
    float earthyMutedness { 0.3f };
    float colorEntropy { 0.5f };

    SynthArchetype archetype { SynthArchetype::AnalogBrass };
    TemporalEraInfo temporalEra;
};

class ImageAnalyzer
{
public:
    ImageAnalyzer() = default;
    ~ImageAnalyzer() = default;

    static ImageAnalysisResult analyzeImage(const juce::Image& image);
    static juce::uint64 computeFNV1a64(const juce::Image::BitmapData& bitmap);

private:
    static float computeSobelEdgeDensity(const juce::Image::BitmapData& bitmap);
    static TemporalEraInfo computeTemporalEra(
        float brightness, float saturation, float complexity,
        float earthyMutedness, float colorEntropy,
        float rRatio, float gRatio, float bRatio,
        float cRatio, float mRatio, float yRatio
    );

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImageAnalyzer)
};
`,
    },
    {
      filename: 'SynthEngine.h',
      language: 'cpp',
      description: 'Polyphonic sound engine with dual detuned oscillators, sub-oscillator, 24dB ladder filter, and envelopes.',
      code: `/*
  ==============================================================================
    SynthEngine.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "ImageAnalyzer.h"

struct VoiceParams
{
    float cutoff { 2000.0f };
    float resonance { 1.0f };
    float attack { 0.01f };
    float decay { 0.5f };
    float sustain { 0.7f };
    float release { 0.3f };
    float lfoRate { 2.0f };
    float lfoDepth { 0.2f };
    float drive { 0.0f };
    SynthArchetype archetype { SynthArchetype::AnalogBrass };
    TemporalEra era { TemporalEra::Era1970s };
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    SynthVoice();
    ~SynthVoice() override = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepare(double sampleRate, int samplesPerBlock);
    void updatePatch(const VoiceParams& params);

private:
    double currentSampleRate { 44100.0 };
    float currentPitchHz { 440.0f };
    float currentVelocity { 0.8f };
    VoiceParams currentParams;

    juce::dsp::Oscillator<float> osc1, osc2, subOsc;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR ampEnv;
    juce::ADSR filterEnv;
    float lfoPhase { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};

class PhotoSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
`,
    },
    {
      filename: 'PluginProcessor.h',
      language: 'cpp',
      description: 'AudioProcessor with APVTS parameter automation, multi-slot morph state, and live oscilloscope FIFO.',
      code: `/*
  ==============================================================================
    PluginProcessor.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ImageAnalyzer.h"
#include "SynthEngine.h"

class PhotoSynthAudioProcessor : public juce::AudioProcessor
{
public:
    PhotoSynthAudioProcessor();
    ~PhotoSynthAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Photo Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadSlotAImage(const juce::Image& img);
    void loadSlotBImage(const juce::Image& img);
    ImageAnalysisResult getLatestAnalysis() const { return currentAnalysis; }

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int scopeBufferSize = 512;
    void getScopeData(float* destination, int numSamples);

private:
    juce::Synthesiser synth;
    ImageAnalysisResult analysisSlotA;
    ImageAnalysisResult analysisSlotB;
    ImageAnalysisResult currentAnalysis;

    juce::dsp::Limiter<float> masterLimiter;
    juce::dsp::Chorus<float> masterChorus;

    float scopeBuffer[scopeBufferSize] { 0.0f };
    int scopeBufferWriteIndex { 0 };
    juce::CriticalSection scopeLock;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateMorphState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioProcessor)
};
`,
    },
    {
      filename: 'PluginEditor.h',
      language: 'cpp',
      description: '1:1 UI split layout: Timbre DNA Gauges, Drag & Drop Center Zone, Oscilloscope, Keyboard & Rotary Knobs.',
      code: `/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

class RadialGaugeComponent : public juce::Component
{
public:
    RadialGaugeComponent(const juce::String& labelText, juce::Colour ringColor);
    void setValue(float newValue) { targetValue = newValue; repaint(); }
    void paint(juce::Graphics& g) override;

private:
    juce::String label;
    juce::Colour color;
    float targetValue { 0.5f };
};

class OscilloscopeComponent : public juce::Component
{
public:
    OscilloscopeComponent() = default;
    void pushData(const float* samples, int numSamples);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> waveformData { std::vector<float>(256, 0.0f) };
};

class PhotoSynthAudioEditor  : public juce::AudioProcessorEditor,
                                public juce::FileDragAndDropTarget,
                                public juce::Timer
{
public:
    PhotoSynthAudioEditor (PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void timerCallback() override;

private:
    PhotoSynthAudioProcessor& audioProcessor;
    SynthLookAndFeel customLookAndFeel;

    RadialGaugeComponent colorGauge       { "COLOR",       juce::Colour(0xffec4899) };
    RadialGaugeComponent brightnessGauge  { "BRIGHTNESS",  juce::Colour(0xfff59e0b) };
    RadialGaugeComponent saturationGauge  { "SATURATION",  juce::Colour(0xffa855f7) };
    RadialGaugeComponent complexityGauge  { "COMPLEXITY",  juce::Colour(0xff06b6d4) };

    juce::Image currentLoadedImage;
    OscilloscopeComponent oscilloscope;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    juce::Slider cutoffKnob, resKnob, lfoRateKnob, lfoDepthKnob;
    juce::Slider volKnob, attackKnob, decayKnob, sustainKnob, releaseKnob, driveKnob;
    juce::ToggleButton limiterButton { "LIMITER" };

    juce::Slider morphSlider;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton deepDiveButton { "DEEP DIVE" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioEditor)
};
`,
    },
  ];
}
