/*
  ==============================================================================
    SynthEngine.cpp
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#include "SynthEngine.h"
#include <cmath>

SynthVoice::SynthVoice()
{
    // Initialize Oscillators with anti-aliasing waveshapes
    osc1.initialise([](float x) { return std::sin(x); });
    osc2.initialise([](float x) { return (x / juce::MathConstants<float>::pi); }); // Saw wave
    subOsc.initialise([](float x) { return x < 0.0f ? -1.0f : 1.0f; });            // Square wave
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PhotoSynthSound*>(sound) != nullptr;
}

void SynthVoice::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 2 };

    osc1.prepare(spec);
    osc2.prepare(spec);
    subOsc.prepare(spec);

    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.prepare(spec);

    ampEnv.setSampleRate(sampleRate);
    filterEnv.setSampleRate(sampleRate);
}

void SynthVoice::updatePatch(const VoiceParams& params)
{
    currentParams = params;

    // Filter frequency & resonance
    filter.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, params.cutoff));
    filter.setResonance(juce::jlimit(0.1f, 10.0f, params.resonance));

    // ADSR settings
    juce::dsp::ADSR::Parameters ampParams;
    ampParams.attack  = juce::jmax(0.001f, params.attack);
    ampParams.decay   = juce::jmax(0.01f, params.decay);
    ampParams.sustain = juce::jlimit(0.0f, 1.0f, params.sustain);
    ampParams.release = juce::jmax(0.01f, params.release);
    ampEnv.setParameters(ampParams);

    juce::dsp::ADSR::Parameters fParams;
    fParams.attack  = juce::jmax(0.005f, params.attack * 0.8f);
    fParams.decay   = juce::jmax(0.02f, params.decay * 1.2f);
    fParams.sustain = 0.4f;
    fParams.release = juce::jmax(0.02f, params.release * 0.8f);
    filterEnv.setParameters(fParams);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    currentVelocity = velocity;
    currentPitchHz = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    // Subtle archetype pitch & oscillator detune
    float detuneRatio = 1.004f; // 7 cents spread
    if (currentParams.archetype == SynthArchetype::DigitalFmBells) {
        detuneRatio = 1.000f; // Pure crystalline tuning
    } else if (currentParams.archetype == SynthArchetype::AnalogBrass) {
        detuneRatio = 1.006f; // Fat analog spread
    }

    osc1.setFrequency(currentPitchHz);
    osc2.setFrequency(currentPitchHz * detuneRatio);
    subOsc.setFrequency(currentPitchHz * 0.5f); // 1 octave below

    ampEnv.noteOn();
    filterEnv.noteOn();
    lfoPhase = 0.0f;
}

void SynthVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff) {
        ampEnv.noteOff();
        filterEnv.noteOff();
    } else {
        clearCurrentNote();
        ampEnv.reset();
        filterEnv.reset();
    }
}

void SynthVoice::pitchWheelMoved(int) {}
void SynthVoice::controllerMoved(int, int) {}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive()) return;

    juce::AudioBuffer<float> voiceBuffer;
    voiceBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    voiceBuffer.clear();

    juce::dsp::AudioBlock<float> block(voiceBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Render oscillators
    osc1.process(context);

    // Blend in second oscillator & sub
    juce::AudioBuffer<float> osc2Buffer;
    osc2Buffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    osc2Buffer.clear();
    juce::dsp::AudioBlock<float> osc2Block(osc2Buffer);
    juce::dsp::ProcessContextReplacing<float> osc2Context(osc2Block);
    osc2.process(osc2Context);

    // Apply oscillator mix based on archetype
    float osc2Mix = 0.6f;
    float subMix = 0.25f;
    if (currentParams.archetype == SynthArchetype::DigitalFmBells) {
        osc2Mix = 0.35f;
        subMix = 0.1f;
    } else if (currentParams.archetype == SynthArchetype::AnalogBrass) {
        osc2Mix = 0.85f;
        subMix = 0.35f;
    }

    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) {
        voiceBuffer.addFrom(ch, 0, osc2Buffer, ch, 0, numSamples, osc2Mix);
    }

    // Process ladder filter
    filter.process(context);

    // Calculate LFO & Envelope per sample
    float lfoInc = (float)(2.0 * juce::MathConstants<double>::pi * currentParams.lfoRate / currentSampleRate);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float envGain = ampEnv.getNextSample();
        float fEnv = filterEnv.getNextSample();
        float lfoVal = std::sin(lfoPhase) * currentParams.lfoDepth;
        lfoPhase += lfoInc;
        if (lfoPhase > juce::MathConstants<float>::twoPi) {
            lfoPhase -= juce::MathConstants<float>::twoPi;
        }

        // Apply gain, velocity, LFO modulation, and filter envelope
        float sampleGain = envGain * currentVelocity * (1.0f + lfoVal * 0.3f);

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            float s = voiceBuffer.getSample(channel, sample) * sampleGain * 0.25f;

            // Soft saturation drive
            if (currentParams.drive > 0.05f) {
                float driven = s * (1.0f + currentParams.drive * 3.0f);
                s = std::tanh(driven);
            }

            outputBuffer.addSample(channel, startSample + sample, s);
        }
    }

    if (!ampEnv.isActive()) {
        clearCurrentNote();
    }
}
