/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Photo Synth
    100% Accurate Layout & Aesthetics to Paint A Synth Prototype
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

// Custom Radial Gauge component matching web app's RadialGauge.tsx
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

// Custom Oscilloscope component matching web app's Oscilloscope.tsx
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

    // Left Column: TIMBRE DNA Radial Gauges
    RadialGaugeComponent colorGauge       { "COLOR",       juce::Colour(0xffec4899) }; // Pink
    RadialGaugeComponent brightnessGauge  { "BRIGHTNESS",  juce::Colour(0xfff59e0b) }; // Amber
    RadialGaugeComponent saturationGauge  { "SATURATION",  juce::Colour(0xffa855f7) }; // Purple
    RadialGaugeComponent complexityGauge  { "COMPLEXITY",  juce::Colour(0xff06b6d4) }; // Cyan

    // Center Column: Dropzone, Oscilloscope & Virtual Keyboard
    juce::Image currentLoadedImage;
    OscilloscopeComponent oscilloscope;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent;

    // Right Column: Manual Override Knobs
    juce::Slider cutoffKnob, resKnob, lfoRateKnob, lfoDepthKnob;
    juce::Slider volKnob, attackKnob, decayKnob, sustainKnob, releaseKnob, driveKnob;
    juce::ToggleButton limiterButton { "LIMITER" };

    // Top Bar: Morph Slider & Preset Selector
    juce::Slider morphSlider;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton deepDiveButton { "DEEP DIVE" };

    // Attachments for thread-safe APVTS parameter sync
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
