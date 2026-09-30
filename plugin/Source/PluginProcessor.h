/*
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
#include "EffectsChain.h"
#include "PresetFactory.h"

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

    // Image Slot Loading & Morphing
    void loadSlotAImage(const juce::Image& img, const juce::String& name = {});
    void loadSlotBImage(const juce::Image& img, const juce::String& name = {});
    void loadSlotAPreset(int presetIndex);
    void loadSlotBPreset(int presetIndex);

    juce::Image getImageA() const { return imageSlotA; }
    juce::Image getImageB() const { return imageSlotB; }
    juce::Image getBlendedImage();

    juce::String getSlotAName() const { return slotAName; }
    juce::String getSlotBName() const { return slotBName; }
    juce::String getImageSlotAAsDataUrl() const;
    juce::String getImageSlotBAsDataUrl() const;
    void syncSlotImageFromFrontend(const juce::String& slot, const juce::String& dataUrl, const juce::String& name);

    ImageAnalysisResult getLatestAnalysis() const { return currentAnalysis; }
    ImageAnalysisResult getAnalysisA() const { return analysisSlotA; }
    ImageAnalysisResult getAnalysisB() const { return analysisSlotB; }

    juce::AudioProcessorValueTreeState apvts;

    // Real-time oscilloscope FIFO buffer for Editor
    static constexpr int scopeBufferSize = 512;
    void getScopeData(float* destination, int numSamples);

    // Keyboard trigger helper for UI virtual piano
    void noteOn(int midiNoteNumber, float velocity);
    void noteOff(int midiNoteNumber);
    void allNotesOff();

    // Preset save / load helpers
    struct UserPreset
    {
        juce::String name;
        juce::String description;
        juce::Image imageA;
        juce::Image imageB;
        juce::String slotAName;
        juce::String slotBName;
        std::unique_ptr<juce::XmlElement> xmlState;
    };

    void saveCurrentPreset(const juce::String& name, const juce::String& desc = {});
    juce::StringArray getUserPresetNames() const;
    void loadUserPreset(const juce::String& name);
    void loadUserPresetByIndex(int index);
    void saveUserPresetsToDisk();
    void loadUserPresetsFromDisk();

    // Analysis and parameters synchronization
    void updateParametersFromAnalysis();
    void applyAnalysisToParameters(const ImageAnalysisResult& analysis);

    // BPM Support
    float getBpm() const { return currentBpm; }
    void setBpm(float newBpm);

private:
    juce::Synthesiser synth;
    juce::MidiBuffer internalMidiBuffer;
    juce::CriticalSection midiLock;

    juce::Image imageSlotA;
    juce::Image imageSlotB;
    juce::Image blendedImage;
    juce::String slotAName { "Synthwave Sunset" };
    juce::String slotBName { "Obsidian Crypt" };

    ImageAnalysisResult analysisSlotA;
    ImageAnalysisResult analysisSlotB;
    ImageAnalysisResult currentAnalysis;

    photosynth::EffectsChain effectsChain;
    juce::dsp::Limiter<float> masterLimiter;

    // Thread-safe Scope FIFO
    float scopeBuffer[scopeBufferSize] { 0.0f };
    int scopeBufferWriteIndex { 0 };
    juce::CriticalSection scopeLock;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateMorphState();
    void updateBlendedImage();

    float currentBpm { 120.0f };
    juce::StringArray userPresetNames;
    std::vector<UserPreset> userPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioProcessor)
};
