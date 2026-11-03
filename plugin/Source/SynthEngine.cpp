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
    prepare(44100.0, 512);
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

    // Known, silent starting state. The patch (currentParams) is intentionally NOT
    // overwritten here: prepare() can be called by the host at any time
    // (sample-rate change, re-activation) and must not throw away the active preset.
    ampEnv.reset();
    filterEnv.reset();
    phase1 = phase2 = phaseSub = phaseMod = lfoPhase = 0.0f;
    filterS1[0] = filterS1[1] = 0.0f;
    filterS2[0] = filterS2[1] = 0.0f;
    pitchBendRatio = 1.0f;

    sanitise(currentParams);
    applyEnvelopeParams();
}

void SynthVoice::sanitise(VoiceParams& p)
{
    auto finiteOr = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };

    p.cutoff     = finiteOr(p.cutoff, 2400.0f);
    if (p.cutoff <= 20.0f || p.cutoff > 20000.0f) p.cutoff = 2400.0f;
    p.resonance  = finiteOr(p.resonance, 1.2f);
    if (p.resonance <= 0.0f) p.resonance = 1.2f;
    p.attack     = finiteOr(p.attack, 0.030f);
    if (p.attack <= 0.0f) p.attack = 0.030f;
    p.decay      = finiteOr(p.decay, 0.35f);
    if (p.decay <= 0.0f) p.decay = 0.35f;
    p.sustain    = finiteOr(p.sustain, 0.70f);
    if (p.sustain <= 0.0f) p.sustain = 0.70f;
    p.release    = finiteOr(p.release, 0.45f);
    if (p.release <= 0.0f) p.release = 0.45f;
    p.lfoRate    = juce::jlimit(0.0f, 50.0f, finiteOr(p.lfoRate, 2.0f));
    p.lfoDepth   = juce::jlimit(0.0f, 1.0f, finiteOr(p.lfoDepth, 0.25f));
    p.drive      = juce::jlimit(0.0f, 1.0f, finiteOr(p.drive, 0.0f));
    p.color      = finiteOr(p.color, 88.0f);
    p.brightness = finiteOr(p.brightness, 45.0f);
    p.saturation = finiteOr(p.saturation, 83.0f);
    p.complexity = finiteOr(p.complexity, 52.0f);

    if (p.archetype != SynthArchetype::AnalogBrass &&
        p.archetype != SynthArchetype::DigitalFmBells &&
        p.archetype != SynthArchetype::HybridWavetable)
    {
        p.archetype = SynthArchetype::HybridWavetable;
    }
}

void SynthVoice::applyEnvelopeParams()
{
    // ADSR settings: 12ms attack floor prevents waveform step clicks
    juce::ADSR::Parameters ampParams;
    ampParams.attack  = juce::jmax(0.012f, currentParams.attack);
    ampParams.decay   = juce::jmax(0.02f, currentParams.decay);
    ampParams.sustain = juce::jlimit(0.05f, 1.0f, currentParams.sustain);
    ampParams.release = juce::jmax(0.02f, currentParams.release);
    ampEnv.setParameters(ampParams);

    juce::ADSR::Parameters fParams;
    fParams.attack  = juce::jmax(0.012f, currentParams.attack * 0.8f);
    fParams.decay   = juce::jmax(0.02f, currentParams.decay * 1.2f);
    fParams.sustain = 0.5f;
    fParams.release = juce::jmax(0.02f, currentParams.release * 0.8f);
    filterEnv.setParameters(fParams);
}

void SynthVoice::updatePatch(const VoiceParams& params)
{
    currentParams = params;
    sanitise(currentParams);
    applyEnvelopeParams();
}

float SynthVoice::pitchWheelToRatio(int pitchWheelValue)
{
    // 14-bit pitch wheel: 0..16383, centre 8192
    const float normalised = juce::jlimit(-1.0f, 1.0f, (float)(pitchWheelValue - 8192) / 8192.0f);
    return std::pow(2.0f, normalised * pitchBendRangeSemitones / 12.0f);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    currentVelocity = juce::jlimit(0.05f, 1.0f, velocity);
    currentPitchHz = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    pitchBendRatio = pitchWheelToRatio(currentPitchWheelPosition);

    float sr = (float)(currentSampleRate > 1000.0 ? currentSampleRate : 44100.0);
    phaseIncrement = currentPitchHz * pitchBendRatio / sr;

    // Surge XT pattern: sync active master patch parameters at note onset.
    // masterParams is owned by PhotoSynthEngine and only written on the audio thread.
    if (masterParams != nullptr)
    {
        currentParams = *masterParams;
    }
    else
    {
        // Voice was added without PhotoSynthEngine - it would play its last/local patch
        jassertfalse;
    }

    // Guard against uninitialised state from external DAW keyboards
    sanitise(currentParams);

    // Refresh envelope parameters to ensure active preset envelope is applied immediately
    applyEnvelopeParams();

    // Reset modulation phase for punchy transient attack
    phaseMod = 0.0f;
    lfoPhase = 0.0f;
    filterS1[0] = filterS1[1] = 0.0f;
    filterS2[0] = filterS2[1] = 0.0f;

    ampEnv.noteOn();
    filterEnv.noteOn();
}

void SynthVoice::stopNote(float /*velocity*/, bool allowTailOff)
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

void SynthVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendRatio = pitchWheelToRatio(newPitchWheelValue);
}

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

    float sr = (float)(currentSampleRate > 1000.0 ? currentSampleRate : 44100.0);
    phaseIncrement = currentPitchHz * pitchBendRatio / sr;

    // 1. Color / Timbre DNA Parameter (0.0 to 1.0 or 0 to 360 deg)
    // Directly modulates harmonic unison detune spread & spectral balance
    float normColor = juce::jlimit(0.0f, 1.0f, currentParams.color > 1.0f ? currentParams.color / 360.0f : currentParams.color);
    float detuneSpreadCents = 5.0f + normColor * 25.0f; // 5 to 30 cents detune spread
    float detuneDown = std::pow(2.0f, -detuneSpreadCents / 1200.0f);
    float detuneUp   = std::pow(2.0f,  detuneSpreadCents / 1200.0f);

    float inc1   = phaseIncrement * detuneDown;
    float inc2   = phaseIncrement * detuneUp;
    float incSub = phaseIncrement * 0.5f; // 1 octave below

    // 2. Complexity Parameter (0 to 100): scales FM modulation depth and overtone ratios
    float normComplexity = juce::jlimit(0.0f, 1.0f, currentParams.complexity > 1.0f ? currentParams.complexity / 100.0f : currentParams.complexity);
    float modRatio = 1.0f + std::round(normColor * 3.0f) + (normComplexity > 0.5f ? 1.0f : 0.0f);
    float incMod = phaseIncrement * modRatio;
    float lfoInc = (float)(2.0 * juce::MathConstants<double>::pi * currentParams.lfoRate / sr);

    // 3. Brightness Parameter (0 to 100): dynamically scales base filter cutoff frequency
    float normBrightness = juce::jlimit(0.0f, 1.0f, currentParams.brightness > 1.0f ? currentParams.brightness / 100.0f : currentParams.brightness);
    float brightnessCutoffScale = 0.35f + normBrightness * 1.65f;
    float baseCutoff = juce::jlimit(40.0f, 19500.0f, currentParams.cutoff * brightnessCutoffScale);
    float q = juce::jlimit(0.5f, 8.5f, currentParams.resonance);

    // 4. Saturation Parameter (0 to 100): analog warmth and saturation drive
    float normSaturation = juce::jlimit(0.0f, 1.0f, currentParams.saturation > 1.0f ? currentParams.saturation / 100.0f : currentParams.saturation);
    float effectiveDrive = juce::jlimit(0.0f, 1.0f, currentParams.drive + normSaturation * 0.60f);

    // 5. Per-archetype loudness compensation - same purpose as WebAudioEngine.ts
    //    `engineGainBoost` (equal perceived level across engines). Factors come from
    //    measuring sustained single-note RMS of each C++ archetype (C4, vel 0.8, no drive):
    //    AnalogBrass 0.0610, DigitalFmBells 0.0894, HybridWavetable 0.0796 -> normalised
    //    to HybridWavetable so the default archetype keeps its existing level.
    const float engineGainBoost = currentParams.archetype == SynthArchetype::AnalogBrass    ? 1.304f
                                : currentParams.archetype == SynthArchetype::DigitalFmBells ? 0.890f
                                                                                            : 1.000f;
    const float archetypeGain = 0.30f * engineGainBoost;

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

        // Render stereo oscillator waveforms according to archetype
        float oscSampleL = 0.0f;
        float oscSampleR = 0.0f;

        // Per-branch phase increments (default: shared values computed above)
        float adv1 = inc1, adv2 = inc2, advSub = incSub;

        if (currentParams.archetype == SynthArchetype::AnalogBrass)
        {
            // PolyBLEP anti-aliased triple supersaw with wide stereo placement.
            // WebAudio reference: third saw sits in the SAME octave at freq*1.004
            // (not a sub-octave) with a fixed, non complexity-scaled level.
            const float incSubBrass = phaseIncrement * 1.004f;
            float sawL = getBlepSaw(phase1, inc1);
            float sawR = getBlepSaw(phase2, inc2);
            float subSaw = getBlepSaw(phaseSub, incSubBrass);
            advSub = incSubBrass;

            float subMix = 0.09f;
            oscSampleL = sawL * 0.50f + sawR * 0.25f + subSaw * subMix;
            oscSampleR = sawL * 0.25f + sawR * 0.50f + subSaw * subMix;
        }
        else if (currentParams.archetype == SynthArchetype::DigitalFmBells)
        {
            // 2-Op Crystalline FM Bells with complexity-driven modulation depth
            float fmIndex = (1.0f + fEnv * 2.5f) * (0.5f + normComplexity * 1.5f);
            float mod = std::sin(phaseMod * juce::MathConstants<float>::twoPi) * fmIndex;
            // WebAudio reference uses square carriers; wrap modulated phase into [0,1)
            auto wrap01 = [](float p) { return p - std::floor(p); };
            float carrierL = getBlepSquare(wrap01(phase1 + mod * 0.25f), inc1);
            float carrierR = getBlepSquare(wrap01(phase2 + mod * 0.25f), inc2);
            float bellSub  = getBlepSquare(phaseSub, incSub);

            oscSampleL = carrierL * 0.65f + bellSub * 0.08f;
            oscSampleR = carrierR * 0.65f + bellSub * 0.08f;
        }
        else
        {
            // Hybrid Wavetable: Bandlimited Saw + Detuned Square + Sub-Octave Square
            // WebAudio reference: osc1 at exact pitch, osc2 detuned upward only.
            const float incH1 = phaseIncrement;
            const float incH2 = phaseIncrement * detuneUp * detuneUp;
            float saw1   = getBlepSaw(phase1, incH1);
            float sqr2   = getBlepSquare(phase2, incH2);
            float subSqr = getBlepSquare(phaseSub, incSub);
            adv1 = incH1;
            adv2 = incH2;

            float sqrMix = 0.25f + normComplexity * 0.25f;
            oscSampleL = saw1 * 0.50f + sqr2 * sqrMix + subSqr * 0.09f;
            oscSampleR = saw1 * 0.40f + sqr2 * (sqrMix + 0.10f) + subSqr * 0.09f;
        }

        // Advance oscillator phases
        phase1 += adv1;
        if (phase1 >= 1.0f) phase1 -= 1.0f;
        phase2 += adv2;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        phaseSub += advSub;
        if (phaseSub >= 1.0f) phaseSub -= 1.0f;
        phaseMod += incMod;
        if (phaseMod >= 1.0f) phaseMod -= 1.0f;

        // Apply dynamic cutoff modulation with filter envelope & LFO
        float maxAllowedCutoff = std::min(19500.0f, sr * 0.45f);
        float modCutoff = juce::jlimit(40.0f, maxAllowedCutoff, baseCutoff * (0.85f + fEnv * 0.35f) * (1.0f + lfoVal * 0.20f));
        float g = std::tan(juce::MathConstants<float>::pi * modCutoff / sr);
        float k = 1.0f / q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        // Stereo 2-pole SVF filter calculation with dedicated L/R channels
        float outL = 0.0f;
        float outR = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
        {
            if (!std::isfinite(filterS1[ch]) || !std::isfinite(filterS2[ch])) {
                filterS1[ch] = 0.0f;
                filterS2[ch] = 0.0f;
            }
            float inSample = (ch == 0) ? oscSampleL : oscSampleR;
            float v3 = inSample - filterS2[ch];
            float v1 = a1 * filterS1[ch] + a2 * v3;
            float v2 = filterS2[ch] + a2 * filterS1[ch] + a3 * v3;
            filterS1[ch] = 2.0f * v1 - filterS1[ch];
            filterS2[ch] = 2.0f * v2 - filterS2[ch];
            float vOut = std::isfinite(v2) ? juce::jlimit(-3.0f, 3.0f, v2) : inSample;
            if (ch == 0) outL = vOut; else outR = vOut;
        }

        // Voice envelope and velocity scaling
        float voiceGain = envGain * currentVelocity * archetypeGain;
        float sampleL = outL * voiceGain;
        float sampleR = outR * voiceGain;

        // Soft saturation drive (hyperbolic tangent curve matching Web Prototype)
        if (effectiveDrive > 0.01f) {
            float driveAmount = 1.0f + effectiveDrive * 3.5f;
            sampleL = std::tanh(sampleL * driveAmount) / std::tanh(driveAmount);
            sampleR = std::tanh(sampleR * driveAmount) / std::tanh(driveAmount);
        }

        if (!std::isfinite(sampleL)) sampleL = 0.0f;
        if (!std::isfinite(sampleR)) sampleR = 0.0f;

        outputBuffer.addSample(0, startSample + i, sampleL);
        if (numChannels > 1) {
            outputBuffer.addSample(1, startSample + i, sampleR);
        }
    }
}


//==============================================================================
// PhotoSynthEngine
//==============================================================================

PhotoSynthEngine::PhotoSynthEngine()
{
    buildVoices();
    // Make the engine playable immediately, even before the host calls prepareToPlay().
    prepare(44100.0, 512);
}

void PhotoSynthEngine::buildVoices()
{
    synth.clearVoices();
    synth.clearSounds();

    synth.addSound(new PhotoSynthSound());
    for (int i = 0; i < numVoices; ++i)
    {
        auto* v = new SynthVoice();
        v->setSharedParams(&sharedParams);
        v->updatePatch(sharedParams);
        synth.addVoice(v);
    }

    // Voice stealing so a held DAW chord never silently drops new notes
    synth.setNoteStealingEnabled(true);
}

bool PhotoSynthEngine::validateCustomVoices() const
{
    if (synth.getNumSounds() == 0 || synth.getNumVoices() != numVoices)
        return false;

    auto* sound = synth.getSound(0).get();
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i));
        if (v == nullptr || ! v->canPlaySound(sound))
            return false;
    }
    return true;
}

void PhotoSynthEngine::prepare(double sampleRate, int samplesPerBlock)
{
    const double sr = (sampleRate > 1000.0) ? sampleRate : 44100.0;

    // There is no generic fallback voice in this plugin: if the custom voices are
    // missing or cannot play PhotoSynthSound, juce::Synthesiser would silently drop
    // every note. Detect it loudly and rebuild once.
    if (! validateCustomVoices())
    {
        DBG("PhotoSynthEngine: custom SynthVoice pool invalid ("
            << synth.getNumVoices() << " voices, " << synth.getNumSounds()
            << " sounds) - rebuilding custom voices");
        jassertfalse;
        buildVoices();
    }

    ready = validateCustomVoices();
    if (! ready)
    {
        DBG("PhotoSynthEngine: FAILED to initialise custom SynthVoice engine - notes will be silent");
        jassertfalse;
    }

    synth.setCurrentPlaybackSampleRate(sr);
    synth.allNotesOff(0, false);

    SynthVoice::sanitise(sharedParams);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
        {
            v->setSharedParams(&sharedParams);
            v->updatePatch(sharedParams);
            v->prepare(sr, samplesPerBlock);
        }
    }
}

void PhotoSynthEngine::reset()
{
    synth.allNotesOff(0, false);
}

void PhotoSynthEngine::setPatch(const VoiceParams& params)
{
    sharedParams = params;
    SynthVoice::sanitise(sharedParams);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            v->updatePatch(sharedParams);
}

void PhotoSynthEngine::render(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples)
{
    jassert(ready);
    // juce::Synthesiser walks the MidiBuffer and splits the block at every event,
    // calling noteOn / noteOff / handlePitchWheel / handleController / allNotesOff
    // at the exact sample position, then renders our SynthVoices in between.
    synth.renderNextBlock(buffer, midi, startSample, numSamples);
}
