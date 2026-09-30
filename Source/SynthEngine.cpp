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

    // Keep oscillator phases continuous/smooth to prevent comb-filtering laser clicks on attack
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

// PolyBLEP anti-aliasing residual for quasi-bandlimited waveforms (eliminates digital harshness)
static inline float polyBlep(float t, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt) {
        float x = t / dt;
        return 2.0f * x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        return x * x + 2.0f * x + 1.0f;
    }
    return 0.0f;
}

static inline float getBlepSaw(float phase, float inc)
{
    float naive = 2.0f * phase - 1.0f;
    return naive - polyBlep(phase, inc);
}

static inline float getBlepSquare(float phase, float inc)
{
    float naive = (phase < 0.5f) ? 1.0f : -1.0f;
    naive += polyBlep(phase, inc);
    float phaseShifted = (phase >= 0.5f) ? (phase - 0.5f) : (phase + 0.5f);
    naive -= polyBlep(phaseShifted, inc);
    return naive;
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isVoiceActive()) return;

    int numChannels = outputBuffer.getNumChannels();
    if (numChannels == 0 || numSamples == 0) return;

    // Detune ratios: +/- 8.5 cents detune, and sub-oscillator 1 octave below
    float detuneDown = std::pow(2.0f, -8.5f / 1200.0f);
    float detuneUp   = std::pow(2.0f,  8.5f / 1200.0f);

    float inc1   = (currentPitchHz * detuneDown) / (float)currentSampleRate;
    float inc2   = (currentPitchHz * detuneUp)   / (float)currentSampleRate;
    float incSub = (currentPitchHz * 0.5f)       / (float)currentSampleRate; // 1 octave below
    float incMod = (currentPitchHz * 2.0f)       / (float)currentSampleRate;
    float lfoInc = (float)(2.0 * juce::MathConstants<double>::pi * currentParams.lfoRate / currentSampleRate);

    // SVF Filter parameters: directly from analyzed/overridden cutoff
    float baseCutoff = juce::jlimit(80.0f, 19500.0f, currentParams.cutoff);
    float q = juce::jlimit(0.5f, 7.5f, currentParams.resonance);

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

        // Render stereo oscillator waveforms according to archetype (1:1 with WebAudioEngine prototype)
        float oscSampleL = 0.0f;
        float oscSampleR = 0.0f;

        if (currentParams.archetype == SynthArchetype::AnalogBrass)
        {
            // PolyBLEP anti-aliased triple supersaw with wide stereo placement
            float sawL = getBlepSaw(phase1, inc1);
            float sawR = getBlepSaw(phase2, inc2);
            float subSaw = getBlepSaw(phaseSub, inc1 * 1.003f);

            oscSampleL = sawL * 0.50f + sawR * 0.25f + subSaw * 0.25f;
            oscSampleR = sawL * 0.25f + sawR * 0.50f + subSaw * 0.25f;
        }
        else if (currentParams.archetype == SynthArchetype::DigitalFmBells)
        {
            // 2-Op Crystalline FM Bells with sub-octave support
            float mod = std::sin(phaseMod * juce::MathConstants<float>::twoPi) * (1.0f + fEnv * 2.5f);
            float carrierL = std::sin((phase1 + mod * 0.25f) * juce::MathConstants<float>::twoPi);
            float carrierR = std::sin((phase2 + mod * 0.25f) * juce::MathConstants<float>::twoPi);
            float bellSub  = getBlepSquare(phaseSub, incSub);

            oscSampleL = carrierL * 0.65f + bellSub * 0.25f;
            oscSampleR = carrierR * 0.65f + bellSub * 0.25f;
        }
        else
        {
            // Hybrid Wavetable (1:1 with prototype): Bandlimited Saw + Detuned Square + Sub-Octave Square
            float saw1   = getBlepSaw(phase1, inc1);
            float sqr2   = getBlepSquare(phase2, inc2);
            float subSqr = getBlepSquare(phaseSub, incSub);

            oscSampleL = saw1 * 0.45f + sqr2 * 0.35f + subSqr * 0.25f;
            oscSampleR = saw1 * 0.35f + sqr2 * 0.45f + subSqr * 0.25f;
        }

        // Advance oscillator phases
        phase1 += inc1;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += inc2;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        phaseSub += incSub;
        if (phaseSub >= 1.0f) phaseSub -= 1.0f;
        phaseMod += incMod;
        if (phaseMod >= 1.0f) phaseMod -= 1.0f;

        // Apply dynamic cutoff modulation with filter envelope & LFO (musical & open)
        float modCutoff = juce::jlimit(80.0f, 19500.0f, baseCutoff * (0.85f + fEnv * 0.35f) * (1.0f + lfoVal * 0.20f));
        float g = std::tan(juce::MathConstants<float>::pi * modCutoff / (float)currentSampleRate);
        float k = 1.0f / q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        // Stereo 2-pole SVF filter calculation with dedicated L/R channels
        float outL = 0.0f;
        float outR = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            float inSample = (ch == 0) ? oscSampleL : oscSampleR;
            float v3 = inSample - filterS2[ch];
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

        // Soft saturation drive (hyperbolic tangent curve matching Web Prototype)
        if (currentParams.drive > 0.01f) {
            float driveAmount = 1.0f + currentParams.drive * 3.5f;
            sampleL = std::tanh(sampleL * driveAmount) / std::tanh(driveAmount);
            sampleR = std::tanh(sampleR * driveAmount) / std::tanh(driveAmount);
        }

        outputBuffer.addSample(0, startSample + i, sampleL);
        if (numChannels > 1) {
            outputBuffer.addSample(1, startSample + i, sampleR);
        }
    }
}
