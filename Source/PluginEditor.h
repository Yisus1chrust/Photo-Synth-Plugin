/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Photo Synth
    100% 1:1 Accurate Layout & Features matching Web Prototype
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

// 1. Radial Gauge Component
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

// 2. Real-time Oscilloscope Component
class OscilloscopeComponent : public juce::Component
{
public:
    OscilloscopeComponent() = default;
    void pushData(const float* samples, int numSamples);
    void paint(juce::Graphics& g) override;

private:
    std::vector<float> waveformData { std::vector<float>(256, 0.0f) };
};

// 3. Virtual Piano Keyboard Component
class VirtualPianoKeyboard : public juce::Component
{
public:
    VirtualPianoKeyboard(PhotoSynthAudioProcessor& processor);
    ~VirtualPianoKeyboard() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    PhotoSynthAudioProcessor& processor;
    int currentNote { -1 };
    int getNoteAtPosition(juce::Point<int> pos);
    bool isBlackKey(int midiNote);
};

// 4. MST Logo Component
class LogoComponent : public juce::Component
{
public:
    void paint(juce::Graphics& g) override;
};

// 5. Effects Modal Overlay Component
class EffectsModalComponent : public juce::Component
{
public:
    EffectsModalComponent(PhotoSynthAudioProcessor& processor, SynthLookAndFeel& lf, std::function<void()> onClose);
    ~EffectsModalComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhotoSynthAudioProcessor& processor;
    std::function<void()> closeCallback;

    juce::TextButton closeBtn { "CLOSE" };
    juce::TextButton xBtn { "X" };
    juce::TextButton allOnBtn { "ALL ON" };
    juce::TextButton allOffBtn { "ALL OFF" };

    // 6 Effect Modules
    juce::ToggleButton delayToggle { "DELAY" };
    juce::Slider delayTimeKnob, delayFbKnob, delayMixKnob;

    juce::ToggleButton reverbToggle { "REVERB" };
    juce::Slider reverbSizeKnob, reverbDecayKnob, reverbMixKnob;

    juce::ToggleButton chorusToggle { "CHORUS" };
    juce::Slider chorusRateKnob, chorusDepthKnob, chorusMixKnob;

    juce::ToggleButton phaserToggle { "PHASER" };
    juce::Slider phaserRateKnob, phaserDepthKnob, phaserFbKnob;

    juce::ToggleButton flangerToggle { "FLANGER" };
    juce::Slider flangerRateKnob, flangerDepthKnob, flangerMixKnob;

    juce::ToggleButton distToggle { "DISTORTION" };
    juce::Slider distDriveKnob, distMixKnob;

    // Attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
};

// 6. Save Preset Modal Overlay Component
class SavePresetModalComponent : public juce::Component
{
public:
    SavePresetModalComponent(PhotoSynthAudioProcessor& processor, std::function<void()> onClose, std::function<void(const juce::String&)> onSaved);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhotoSynthAudioProcessor& processor;
    std::function<void()> closeCallback;
    std::function<void(const juce::String&)> saveCallback;

    juce::TextEditor nameInput;
    juce::TextEditor descInput;
    juce::TextButton saveBtn { "SAVE PRESET" };
    juce::TextButton cancelBtn { "CANCEL" };
};

// 7. Deep Dive Modal Overlay Component
class DeepDiveModalComponent : public juce::Component
{
public:
    DeepDiveModalComponent(PhotoSynthAudioProcessor& processor, std::function<void()> onClose);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PhotoSynthAudioProcessor& processor;
    std::function<void()> closeCallback;
    juce::TextButton closeBtn { "CLOSE" };
};

// Main Editor
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

    void mouseUp(const juce::MouseEvent& e) override;

private:
    PhotoSynthAudioProcessor& audioProcessor;
    SynthLookAndFeel customLookAndFeel;

    // Header Components
    LogoComponent logoBox;
    juce::TextButton autoSweepBtn { ">" };
    bool isAutoSweeping { false };
    float sweepDirection { 1.0f };

    juce::ComboBox slotABox;
    juce::Slider morphSlider;
    juce::ComboBox slotBBox;

    juce::ComboBox presetBox;
    juce::TextButton savePresetBtn { "SAVE" };
    juce::TextButton effectsBtn { "EFFECTS" };
    juce::TextButton deepDiveBtn { "DEEP DIVE" };
    juce::TextButton exportBtn { "EXPORT VST" };

    // Left Column: TIMBRE DNA Radial Gauges
    RadialGaugeComponent colorGauge       { "COLOR",       juce::Colour(0xffec4899) }; // Pink
    RadialGaugeComponent brightnessGauge  { "BRIGHTNESS",  juce::Colour(0xfff59e0b) }; // Amber
    RadialGaugeComponent saturationGauge  { "SATURATION",  juce::Colour(0xffa855f7) }; // Purple
    RadialGaugeComponent complexityGauge  { "COMPLEXITY",  juce::Colour(0xff06b6d4) }; // Cyan

    // Center Column: Center Image, Era Pill, Oscilloscope & Virtual Piano
    juce::Rectangle<int> imageBounds;
    OscilloscopeComponent oscilloscope;
    VirtualPianoKeyboard pianoKeyboard;
    juce::String currentEraText { "ERA: 1980s: Physical & Cathedral" };

    // Right Column: Manual Override Knobs
    juce::Slider cutoffKnob, resKnob, lfoRateKnob, lfoDepthKnob;
    juce::Slider volKnob, attackKnob, decayKnob, sustainKnob, releaseKnob;
    juce::ToggleButton limiterButton { "LIMITER" };

    // Modals
    std::unique_ptr<EffectsModalComponent> effectsModal;
    std::unique_ptr<SavePresetModalComponent> savePresetModal;
    std::unique_ptr<DeepDiveModalComponent> deepDiveModal;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttach;

    void openImageFileChooser(bool isSlotA);
    void updateEraText();
    void refreshPresetDropdown();

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioEditor)
};
