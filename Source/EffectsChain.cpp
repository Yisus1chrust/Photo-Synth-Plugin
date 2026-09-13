/*
  ==============================================================================
    EffectsChain.cpp
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#include "EffectsChain.h"

namespace photosynth
{
    void EffectsChain::prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = (float) spec.sampleRate;
        delayLine.prepare(spec);
        reverb.prepare(spec);
        chorus.prepare(spec);
        phaser.prepare(spec);
        flanger.prepare(spec);
        reset();
    }

    void EffectsChain::reset()
    {
        delayLine.reset();
        reverb.reset();
        chorus.reset();
        phaser.reset();
        flanger.reset();
    }

    void EffectsChain::updateParameters(const EffectsParameters& params)
    {
        currentParams = params;

        // Delay setup
        float timeSec = juce::jlimit(0.01f, 2.0f, params.delayTimeMs * 0.001f);
        delayTimeSamples = timeSec * sampleRate;

        // Reverb setup
        juce::Reverb::Parameters rv;
        rv.roomSize   = juce::jlimit(0.0f, 1.0f, params.reverbRoomSize);
        rv.damping    = 0.4f;
        rv.wetLevel   = params.reverbEnabled ? params.reverbMix : 0.0f;
        rv.dryLevel   = 1.0f;
        rv.width      = 0.9f;
        rv.freezeMode = 0.0f;
        reverb.setParameters(rv);

        // Chorus setup
        chorus.setRate(juce::jlimit(0.1f, 10.0f, params.chorusRateHz));
        chorus.setDepth(juce::jlimit(0.0f, 1.0f, params.chorusDepth));
        chorus.setCentreDelay(7.0f);
        chorus.setFeedback(0.15f);
        chorus.setMix(params.chorusEnabled ? params.chorusMix : 0.0f);

        // Phaser setup
        phaser.setRate(juce::jlimit(0.1f, 8.0f, params.phaserRateHz));
        phaser.setDepth(params.phaserEnabled ? params.phaserDepth : 0.0f);
        phaser.setCentreFrequency(600.0f);
        phaser.setFeedback(juce::jlimit(0.0f, 0.8f, params.phaserFeedback));
        phaser.setMix(params.phaserEnabled ? params.phaserMix : 0.0f);

        // Flanger setup
        flanger.setRate(juce::jlimit(0.05f, 5.0f, params.flangerRateHz));
        flanger.setDepth(params.flangerEnabled ? params.flangerDepth : 0.0f);
        flanger.setCentreDelay(3.0f);
        flanger.setFeedback(0.4f);
        flanger.setMix(params.flangerEnabled ? params.flangerMix : 0.0f);
    }

    float EffectsChain::processDistortionSample(float x) const
    {
        float drive = 1.0f + currentParams.distortionDrive * 15.0f;
        float driven = x * drive;
        return std::tanh(driven);
    }

    void EffectsChain::processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0)
            return;

        int numCh = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        // 1. Distortion (if enabled)
        if (currentParams.distortionEnabled && currentParams.distortionMix > 0.001f)
        {
            float mix = currentParams.distortionMix;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* channelData = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float dry = channelData[i];
                    float wet = processDistortionSample(dry);
                    channelData[i] = dry * (1.0f - mix) + wet * mix;
                }
            }
        }

        // 2. Stereo Delay (if enabled)
        if (currentParams.delayEnabled && currentParams.delayMix > 0.001f)
        {
            float mix = currentParams.delayMix;
            float fb = currentParams.delayFeedback;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* dry = buffer.getWritePointer(ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    float delayed = delayLine.popSample(ch, delayTimeSamples, true);
                    delayLine.pushSample(ch, dry[i] + delayed * fb);
                    dry[i] = dry[i] * (1.0f - mix) + delayed * mix;
                }
            }
        }

        // 3. Modulation Effects & Reverb
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        if (currentParams.chorusEnabled)
            chorus.process(context);

        if (currentParams.phaserEnabled)
            phaser.process(context);

        if (currentParams.flangerEnabled)
            flanger.process(context);

        if (currentParams.reverbEnabled)
            reverb.process(context);
    }
}
