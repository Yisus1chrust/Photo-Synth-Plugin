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
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PhotoSynthSound*>(sound) != nullptr;
}

void SynthVoice::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    ampEnv.setSampleRate(currentSampleRate);
    filterEnv.setSampleRate(currentSampleRate);
    filterS1[0] = filterS1[1] = 0.0f;
    filterS2[0] = filterS2[1] = 0.0f;

    // Set immediate playable default ADSR parameters
    juce::ADSR::Parameters ampParams;
    ampParams.attack  = 0.015f;
    ampParams.decay   = 0.35f;
    ampParams.sustain = 0.80f;
    ampParams.release = 0.30f;
    ampEnv.setParameters(ampParams);

    juce::ADSR::Parameters fParams;
    fParams.attack  = 0.015f;
    fParams.decay   = 0.40f;
    fParams.sustain = 0.60f;
    fParams.release = 0.30f;
    filterEnv.setParameters(fParams);
}

void SynthVoice::updatePatch(const VoiceParams& params)
{
    currentParams = params;

    // ADSR settings: 12ms attack floor prevents waveform step clicks
    juce::ADSR::Parameters ampParams;
    ampParams.attack  = juce::jmax(0.012f, params.attack);
    ampParams.decay   = juce::jmax(0.02f, params.decay);
    ampParams.sustain = juce::jlimit(0.05f, 1.0f, params.sustain);
    ampParams.release = juce::jmax(0.02f, params.release);
    ampEnv.setParameters(ampParams);

    juce::ADSR::Parameters fParams;
    fParams.attack  = juce::jmax(0.012f, params.attack * 0.8f);
    fParams.decay   = juce::jmax(0.02f, params.decay * 1.2f);
    fParams.sustain = 0.5f;
    fParams.release = juce::jmax(0.02f, params.release * 0.8f);
    filterEnv.setParameters(fParams);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    currentVelocity = juce::jlimit(0.1f, 1.0f, velocity);
    currentPitchHz = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

    phase1 = 0.0f;
    phase2 = 0.0f;
    phaseSub = 0.0f;
    phaseMod = 0.0f;
    lfoPhase = 0.0f;
    filterS1[0] = filterS1[1] = 0.0f;
    filterS2[0] = filterS2[1] = 0.0f;

    ampEnv.noteOn();
    filterEnv.noteOn();
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

    int numChannels = outputBuffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0) return;

    float detuneRatio = 1.009f;
    if (currentParams.archetype == SynthArchetype::DigitalFmBells) {
        detuneRatio = 1.0f;
    } else if (currentParams.archetype == SynthArchetype::AnalogBrass) {
        detuneRatio = 1.009f;
    }

    float inc1 = currentPitchHz / (float)currentSampleRate;
    float inc2 = (currentPitchHz * detuneRatio) / (float)currentSampleRate;
    float incSub = (currentPitchHz * 0.5f) / (float)currentSampleRate;
    float incMod = (currentPitchHz * 3.0f) / (float)currentSampleRate;
    float lfoInc = (float)(2.0 * juce::MathConstants<double>::pi * currentParams.lfoRate / currentSampleRate);

    // SVF Filter parameters: directly from analyzed/overridden cutoff
    float baseCutoff = juce::jlimit(40.0f, 19500.0f, currentParams.cutoff);
    float q = juce::jlimit(0.5f, 9.5f, currentParams.resonance);

    for (int i = 0; i < numSamples; ++i)
    {
        float envGain = ampEnv.getNextSample();
        if (!ampEnv.isActive()) {
            clearCurrentNote();
            break;
        }

        float fEnv = filterEnv.getNextSample();
        float lfoVal = std::sin(lfoPhase) * currentParams.lfoDepth;
        lfoPhase += lfoInc;
        if (lfoPhase > juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        // Render oscillator waveforms according to archetype
        float oscSample = 0.0f;
        if (currentParams.archetype == SynthArchetype::AnalogBrass)
        {
            // Polyphonic dual detuned saw + sub square
            float saw1 = 2.0f * phase1 - 1.0f;
            float saw2 = 2.0f * phase2 - 1.0f;
            float sqSub = (phaseSub < 0.5f) ? 0.7f : -0.7f;
            oscSample = saw1 * 0.45f + saw2 * 0.40f + sqSub * 0.25f;
        }
        else if (currentParams.archetype == SynthArchetype::DigitalFmBells)
        {
            // 2-Op FM Bells (Modulator at 3:1 ratio with dynamic decay envelope)
            float mod = std::sin(phaseMod * juce::MathConstants<float>::twoPi) * (1.0f + fEnv * 4.0f);
            float carrier = std::sin((phase1 + mod * 0.35f) * juce::MathConstants<float>::twoPi);
            float bellSub = std::sin(phaseSub * juce::MathConstants<float>::twoPi);
            oscSample = carrier * 0.75f + bellSub * 0.25f;
        }
        else
        {
            // Hybrid Wavetable: Triangle / Saw blend
            float tri1 = (phase1 < 0.5f ? 4.0f * phase1 - 1.0f : 3.0f - 4.0f * phase1);
            float saw2 = 2.0f * phase2 - 1.0f;
            oscSample = tri1 * 0.50f + saw2 * 0.50f;
        }

        // Advance phases
        phase1 += inc1;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += inc2;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        phaseSub += incSub;
        if (phaseSub >= 1.0f) phaseSub -= 1.0f;
        phaseMod += incMod;
        if (phaseMod >= 1.0f) phaseMod -= 1.0f;

        // Apply dynamic cutoff modulation
        float modCutoff = juce::jlimit(40.0f, 19500.0f, baseCutoff * (0.6f + fEnv * 0.8f) * (1.0f + lfoVal * 0.25f));
        float g = std::tan(juce::MathConstants<float>::pi * modCutoff / (float)currentSampleRate);
        float k = 1.0f / q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        // Stereo 2-pole SVF filter calculation
        float outL = 0.0f;
        float outR = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            float v3 = oscSample - filterS2[ch];
            float v1 = a1 * filterS1[ch] + a2 * v3;
            float v2 = filterS2[ch] + a2 * filterS1[ch] + a3 * v3;
            filterS1[ch] = 2.0f * v1 - filterS1[ch];
            filterS2[ch] = 2.0f * v2 - filterS2[ch];
            if (ch == 0) outL = v2; else outR = v2;
        }

        // Voice envelope and velocity scaling
        float voiceGain = envGain * currentVelocity * 0.85f;
        float sampleL = outL * voiceGain;
        float sampleR = outR * voiceGain;

        // Soft saturation drive
        if (currentParams.drive > 0.01f) {
            float driveAmount = 1.0f + currentParams.drive * 3.5f;
            sampleL = std::tanh(sampleL * driveAmount);
            sampleR = std::tanh(sampleR * driveAmount);
        }

        outputBuffer.addSample(0, startSample + i, sampleL);
        if (numChannels > 1) {
            outputBuffer.addSample(1, startSample + i, sampleR);
        }
    }
}
