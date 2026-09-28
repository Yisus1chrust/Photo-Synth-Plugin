/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Photo Synth
    Hybrid Audio Plugin Architecture (EVERSYNTH Architecture)
    100% 1:1 Accurate Web Aesthetic with Real-Time Native C++ DSP Engine
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

// Forward declarations
class PhotoSynthAudioProcessor;

#if JUCE_WEB_BROWSER

// ==============================================================================
// Hybrid Web Editor (EVERSYNTH Architecture)
// The UI is the 100% authentic React/Vite Web Interface running offline via
// juce::WebBrowserComponent, linked bi-directionally to the C++ DSP Audio Processor.
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

    // Helper methods for offline resource serving
    juce::File getUiDirectory() const;
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

    // Automation tracking
    float lastReportedBpm { 120.0f };
    std::map<juce::String, float> lastReportedParams;

    // Native file dialog support for image loading
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioEditor)
};

#else

// ==============================================================================
// Fallback Native Vector Editor (when compiled without WebBrowser)
// ==============================================================================

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

struct PianoKeyDef {
    int note { 0 };
    bool isBlack { false };
    juce::Rectangle<float> bounds;
    juce::String keyChar;
};

class VirtualPianoKeyboard : public juce::Component
{
public:
    VirtualPianoKeyboard(PhotoSynthAudioProcessor& processor);
    ~VirtualPianoKeyboard() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void triggerNoteOn(int midiNote, float vel = 0.85f);
    void triggerNoteOff(int midiNote);
private:
    PhotoSynthAudioProcessor& processor;
    int currentMouseNote { -1 };
    std::set<int> activeNotes;
    std::vector<PianoKeyDef> keys;
    int getNoteAtPosition(juce::Point<int> pos);
    static bool isBlackKey(int midiNote);
    int keyPressToMidi(const juce::KeyPress& key);
};

class LogoComponent : public juce::Component
{
public:
    LogoComponent();
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    std::function<void()> onClick;
private:
    juce::Image logoImage;
    bool isHovered { false };
};

class BpmEditorComponent : public juce::Component
{
public:
    BpmEditorComponent();
    BpmEditorComponent(PhotoSynthAudioProcessor& proc);
    void setProcessor(PhotoSynthAudioProcessor& proc);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void updateText();
    juce::Label& getLabel() { return bpmLabel; }
private:
    PhotoSynthAudioProcessor* processor { nullptr };
    juce::Label bpmLabel;
    bool isHovered { false };
};

class PhotoSynthAudioEditor : public juce::AudioProcessorEditor,
                              public juce::FileDragAndDropTarget,
                              public juce::Timer
{
public:
    PhotoSynthAudioEditor(PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void timerCallback() override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
private:
    PhotoSynthAudioProcessor& audioProcessor;
    SynthLookAndFeel customLookAndFeel;
    LogoComponent logoBox;
    juce::TextButton autoSweepBtn { ">" };
    bool isAutoSweeping { false };
    float sweepDirection { 1.0f };
    juce::ComboBox slotABox;
    juce::Slider morphSlider;
    juce::ComboBox slotBBox;
    juce::ComboBox presetBox;
    juce::TextButton savePresetBtn { "SAVE" };
    BpmEditorComponent bpmBox;
    juce::TextButton effectsBtn { "EFFECTS" };
    juce::TextButton deepDiveBtn { "DEEP DIVE" };
    juce::TextButton keyboardToggleBtn { "KEYS" };
    juce::TextButton exportBtn { "EXPORT" };
    RadialGaugeComponent lumGauge { "LUMINANCE", juce::Colour(0xfff59e0b) };
    RadialGaugeComponent satGauge { "SATURATION", juce::Colour(0xffec4899) };
    RadialGaugeComponent edgeGauge { "EDGE DENSITY", juce::Colour(0xff3b82f6) };
    RadialGaugeComponent entropyGauge { "ENTROPY", juce::Colour(0xff10b981) };
    juce::ImageComponent imageDropComponent;
    OscilloscopeComponent oscilloscope;
    juce::TextButton playAudioBtn { "AUDIO TEST" };
    juce::TextButton panicBtn { "PANIC" };
    juce::Slider cutoffKnob, resKnob, driveKnob, lfoRateKnob, lfoDepthKnob;
    juce::Slider attKnob, decKnob, susKnob, relKnob, volKnob;
    juce::ToggleButton limiterToggle { "LIMITER" };
    juce::ComboBox filterTypeBox, waveShapeBox;
    VirtualPianoKeyboard pianoKeyboard;
    bool showPiano { true };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioEditor)
};

#endif
