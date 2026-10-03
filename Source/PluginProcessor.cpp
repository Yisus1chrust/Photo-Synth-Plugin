#include "PluginProcessor.h"
#include "PluginEditor.h"

PhotoSynthAudioProcessor::PhotoSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Color: 0.0 to 100.0, default 80.0 (Harmonic timbre and detuning spread)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "color", 1 }, "Color",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f));

    // 2. Brightness: 0.0 to 100.0, default 45.0 (Filter cutoff scaling)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "brightness", 1 }, "Brightness",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 45.0f));

    // 3. Saturation: 0.0 to 100.0, default 83.0 (Analog warmth and drive)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "saturation", 1 }, "Saturation",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 83.0f));

    // 4. Complexity: 1.0 to 64.0, default 52.0 (FM overtone richness)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "complexity", 1 }, "Complexity",
        juce::NormalisableRange<float> (1.0f, 64.0f, 0.1f), 52.0f));

    // 5. Master Volume: -60.0 to 6.0 dB, default 0.0 dB
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master_vol", 1 }, "Master Vol",
        juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PhotoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // A Software Instrument MUST NOT have audio input channels!
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    // Stereo or mono output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}
#endif

void PhotoSynthAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    for (auto& v : voices)
        v.ampAdsr.setSampleRate (sampleRate);
}

void PhotoSynthAudioProcessor::releaseResources()
{
    for (auto& v : voices)
        v.stop (false);
}

Voice* PhotoSynthAudioProcessor::findFreeVoice()
{
    for (auto& v : voices)
    {
        if (!v.isActive())
            return &v;
    }
    // Voice stealing fallback: return the first voice
    return &voices[0];
}

Voice* PhotoSynthAudioProcessor::findVoiceForNote (int noteNumber)
{
    for (auto& v : voices)
    {
        if (v.isActive() && v.noteNumber == noteNumber)
            return &v;
    }
    return nullptr;
}

// PolyBLEP anti-aliased residual
static inline float polyBlep (float t, float dt)
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt)
    {
        float x = t / dt;
        return 2.0f * x - x * x - 1.0f;
    }
    else if (t > 1.0f - dt)
    {
        float x = (t - 1.0f) / dt;
        return x * x + 2.0f * x + 1.0f;
    }
    return 0.0f;
}

static inline float getBlepSaw (float phase, float inc)
{
    float naive = 2.0f * phase - 1.0f;
    return naive - polyBlep (phase, inc);
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 1. Explicitly clear all output channels so no DAW fallback sound bleeds through
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // 2. Process incoming MIDI messages
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            if (auto* v = findFreeVoice())
                v->start (msg.getNoteNumber(), msg.getFloatVelocity(), currentSampleRate);
        }
        else if (msg.isNoteOff())
        {
            if (auto* v = findVoiceForNote (msg.getNoteNumber()))
                v->stop (true);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices)
                v.stop (false);
        }
    }

    // 3. Read live APVTS parameters atomically
    const float color       = apvts.getRawParameterValue ("color")->load();
    const float brightness  = apvts.getRawParameterValue ("brightness")->load();
    const float saturation  = apvts.getRawParameterValue ("saturation")->load();
    const float complexity  = apvts.getRawParameterValue ("complexity")->load();
    const float masterVolDb = apvts.getRawParameterValue ("master_vol")->load();

    const float masterGain  = juce::Decibels::decibelsToGain (masterVolDb);

    // Color -> Unison detune spread in cents (5 to 35 cents)
    const float detuneCents = 5.0f + (color / 100.0f) * 30.0f;
    const float detuneDown  = std::pow (2.0f, -detuneCents / 1200.0f);
    const float detuneUp    = std::pow (2.0f,  detuneCents / 1200.0f);

    // Brightness -> Filter cutoff frequency (120 Hz to 18 kHz)
    const float normBright  = juce::jlimit (0.0f, 1.0f, brightness / 100.0f);
    const float baseCutoff  = juce::jlimit (80.0f, 18000.0f, 80.0f * std::pow (18000.0f / 80.0f, normBright));

    // Complexity -> SVF resonance factor (Q: 0.707 to 6.5)
    const float normComp    = juce::jlimit (0.0f, 1.0f, (complexity - 1.0f) / 63.0f);
    const float filterQ     = 0.707f + normComp * 5.8f;

    // Saturation -> Analog drive factor
    const float drive       = 1.0f + (saturation / 100.0f) * 4.0f;

    // Filter coefficients (State Variable Filter Lowpass)
    const float sr = (float)(currentSampleRate > 1000.0 ? currentSampleRate : 44100.0);
    const float g  = std::tan (juce::MathConstants<float>::pi * baseCutoff / sr);
    const float k  = 1.0f / filterQ;
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float a2 = g * a1;
    const float a3 = g * a2;

    auto* leftOut  = buffer.getWritePointer (0);
    auto* rightOut = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : leftOut;

    // 4. Render active synth voices into buffer
    for (auto& v : voices)
    {
        if (!v.isActive())
            continue;

        const float inc1 = v.phaseIncrement * detuneDown;
        const float inc2 = v.phaseIncrement * detuneUp;

        for (int i = 0; i < numSamples; ++i)
        {
            const float env = v.ampAdsr.getNextSample();
            if (!v.ampAdsr.isActive())
            {
                v.noteNumber = -1;
                break;
            }

            // Dual detuned PolyBLEP saw oscillators for stereo thickness
            float oscL = getBlepSaw (v.phase, inc1);
            float oscR = getBlepSaw (v.phase, inc2);

            v.phase += inc1;
            if (v.phase >= 1.0f) v.phase -= 1.0f;

            // SVF Filter process on Left
            float v3L = oscL - v.s2;
            float v1L = a1 * v.s1 + a2 * v3L;
            float v2L = v.s2 + a2 * v.s1 + a3 * v3L;
            v.s1 = 2.0f * v1L - v.s1;
            v.s2 = 2.0f * v2L - v.s2;

            float sampleL = v2L * env * v.velocity;
            float sampleR = oscR * env * v.velocity * 0.9f;

            // Saturation drive
            if (drive > 1.05f)
            {
                sampleL = std::tanh (sampleL * drive) / std::tanh (drive);
                sampleR = std::tanh (sampleR * drive) / std::tanh (drive);
            }

            leftOut[i]  += sampleL * 0.35f * masterGain;
            rightOut[i] += sampleR * 0.35f * masterGain;
        }
    }

    // 5. Write to circular Scope FIFO for the Editor Visualizer
    {
        const juce::ScopedLock sl (scopeLock);
        for (int i = 0; i < numSamples; ++i)
        {
            scopeBuffer[scopeWriteIndex] = leftOut[i];
            scopeWriteIndex = (scopeWriteIndex + 1) % scopeSize;
        }
    }
}

void PhotoSynthAudioProcessor::getScopeData (float* dest, int numSamples)
{
    const juce::ScopedLock sl (scopeLock);
    int readIdx = (scopeWriteIndex - numSamples + scopeSize) % scopeSize;
    for (int i = 0; i < numSamples; ++i)
    {
        dest[i] = scopeBuffer[(readIdx + i) % scopeSize];
    }
}

// State Serialization: Save APVTS to Binary XML
void PhotoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

// State Serialization: Restore APVTS from Binary XML
void PhotoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

juce::AudioProcessorEditor* PhotoSynthAudioProcessor::createEditor()
{
    return new PhotoSynthAudioEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
