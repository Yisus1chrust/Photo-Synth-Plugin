/*
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

    // Per-sample oscillator phase accumulators
    float phase1 { 0.0f };
    float phase2 { 0.0f };
    float phaseSub { 0.0f };
    float phaseMod { 0.0f };
    float lfoPhase { 0.0f };

    // 2-pole resonant stereo filter state
    float filterS1[2] { 0.0f, 0.0f };
    float filterS2[2] { 0.0f, 0.0f };

    juce::ADSR ampEnv;
    juce::ADSR filterEnv;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};

class PhotoSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
