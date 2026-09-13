/*
  ==============================================================================
    EffectsChain.h
    Modular Sound Technologies - Photo Synth
    DSP Audio Effects Chain: Delay, Reverb, Chorus, Phaser, Flanger, Distortion
  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

namespace photosynth
{
    struct EffectsParameters
    {
        // Delay
        bool delayEnabled { true };
        float delayTimeMs { 250.0f };
        float delayFeedback { 0.35f };
        float delayMix { 0.25f };

        // Reverb
        bool reverbEnabled { true };
        float reverbRoomSize { 0.65f };
        float reverbDecaySec { 1.5f };
        float reverbMix { 0.30f };

        // Chorus
        bool chorusEnabled { true };
        float chorusRateHz { 1.2f };
        float chorusDepth { 0.35f };
        float chorusMix { 0.25f };

        // Phaser
        bool phaserEnabled { false };
        float phaserRateHz { 0.8f };
        float phaserDepth { 0.5f };
        float phaserFeedback { 0.2f };
        float phaserMix { 0.3f };

        // Flanger
        bool flangerEnabled { false };
        float flangerRateHz { 0.4f };
        float flangerDepth { 0.45f };
        float flangerMix { 0.3f };

        // Distortion
        bool distortionEnabled { false };
        float distortionDrive { 0.2f };
        float distortionMix { 0.25f };
    };

    class EffectsChain
    {
    public:
        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset();
        void updateParameters(const EffectsParameters& params);
        void processBlock(juce::AudioBuffer<float>& buffer);

    private:
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 192000 };
        juce::dsp::Reverb reverb;
        juce::dsp::Chorus<float> chorus;
        juce::dsp::Phaser<float> phaser;
        juce::dsp::Chorus<float> flanger;

        EffectsParameters currentParams;
        float sampleRate { 44100.0f };
        float delayTimeSamples { 11025.0f };

        float processDistortionSample(float x) const;
    };
}
