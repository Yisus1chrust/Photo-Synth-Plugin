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

    // Real-time oscilloscope FIFO buffer for Editor
    static constexpr int scopeBufferSize = 512;
    void getScopeData(float* destination, int numSamples);

private:
    juce::Synthesiser synth;
    ImageAnalysisResult analysisSlotA;
    ImageAnalysisResult analysisSlotB;
    ImageAnalysisResult currentAnalysis;

    juce::dsp::Limiter<float> masterLimiter;
    juce::dsp::Chorus<float> masterChorus;

    // Thread-safe Scope FIFO
    float scopeBuffer[scopeBufferSize] { 0.0f };
    int scopeBufferWriteIndex { 0 };
    juce::CriticalSection scopeLock;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateMorphState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthAudioProcessor)
};
