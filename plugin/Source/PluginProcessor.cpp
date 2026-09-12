/*
  ==============================================================================
    PluginProcessor.cpp
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PhotoSynthAudioProcessor::PhotoSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    synth.addSound (new PhotoSynthSound());
    for (int i = 0; i < 16; ++i) {
        synth.addVoice (new SynthVoice());
    }
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Filter Cutoff & Resonance
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff Frequency", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 10.0f, 0.05f), 1.0f));

    // LFO Modulation
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoRate", 1 }, "LFO Rate", juce::NormalisableRange<float> (0.1f, 20.0f, 0.1f), 2.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoDepth", 1 }, "LFO Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    // Master Volume
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Master Volume", juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -3.0f));

    // ADSR Envelope
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack Time", juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f), 0.01f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "Decay Time", juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.35f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sustain", 1 }, "Sustain Level", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release Time", juce::NormalisableRange<float> (0.01f, 8.0f, 0.01f, 0.35f), 0.4f));

    // Morph Bar [Slot A to Slot B]
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "morph", 1 }, "A/B Morph", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    // Limiter & Drive
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "limiter", 1 }, "Limiter Toggle", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold", 1 }, "Limiter Threshold", juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -3.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Saturation Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    return { params.begin(), params.end() };
}

void PhotoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepare(sampleRate, samplesPerBlock);
    }

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, 2 };
    masterLimiter.setThreshold(*apvts.getRawParameterValue("threshold"));
    masterLimiter.prepare(spec);

    masterChorus.setRate(1.2f);
    masterChorus.setDepth(0.25f);
    masterChorus.setCentreDelay(7.0f);
    masterChorus.setFeedback(0.15f);
    masterChorus.setMix(0.35f);
    masterChorus.prepare(spec);
}

void PhotoSynthAudioProcessor::releaseResources() {}

void PhotoSynthAudioProcessor::loadSlotAImage(const juce::Image& img)
{
    analysisSlotA = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
}

void PhotoSynthAudioProcessor::loadSlotBImage(const juce::Image& img)
{
    analysisSlotB = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
}

void PhotoSynthAudioProcessor::updateMorphState()
{
    float morph = *apvts.getRawParameterValue("morph");

    // Linearly interpolate metrics between Slot A and Slot B
    currentAnalysis.brightness = (1.0f - morph) * analysisSlotA.brightness + morph * analysisSlotB.brightness;
    currentAnalysis.saturation = (1.0f - morph) * analysisSlotA.saturation + morph * analysisSlotB.saturation;
    currentAnalysis.complexity = (1.0f - morph) * analysisSlotA.complexity + morph * analysisSlotB.complexity;
    currentAnalysis.timbreDna  = (1.0f - morph) * analysisSlotA.timbreDna  + morph * analysisSlotB.timbreDna;

    currentAnalysis.archetype = (morph < 0.5f) ? analysisSlotA.archetype : analysisSlotB.archetype;
    currentAnalysis.temporalEra = (morph < 0.5f) ? analysisSlotA.temporalEra : analysisSlotB.temporalEra;
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    updateMorphState();

    VoiceParams p;
    p.cutoff    = *apvts.getRawParameterValue("cutoff");
    p.resonance = *apvts.getRawParameterValue("resonance");
    p.lfoRate   = *apvts.getRawParameterValue("lfoRate");
    p.lfoDepth  = *apvts.getRawParameterValue("lfoDepth");
    p.attack    = *apvts.getRawParameterValue("attack");
    p.decay     = *apvts.getRawParameterValue("decay");
    p.sustain   = *apvts.getRawParameterValue("sustain");
    p.release   = *apvts.getRawParameterValue("release");
    p.drive     = *apvts.getRawParameterValue("drive");
    p.archetype = currentAnalysis.archetype;
    p.era       = currentAnalysis.temporalEra.era;

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i))) {
            voice->updatePatch(p);
        }
    }

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Apply master volume gain
    float volDb = *apvts.getRawParameterValue("volume");
    float masterGain = juce::Decibels::decibelsToGain(volDb, -60.0f) * 0.5f;
    buffer.applyGain(masterGain);

    // Apply Limiter
    if (*apvts.getRawParameterValue("limiter") > 0.5f) {
        float thresholdDb = *apvts.getRawParameterValue("threshold");
        masterLimiter.setThreshold(thresholdDb);
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        masterLimiter.process(context);
    }

    // Capture to oscilloscope FIFO
    {
        const juce::ScopedLock sl(scopeLock);
        const float* leftChannel = buffer.getReadPointer(0);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            scopeBuffer[scopeBufferWriteIndex] = leftChannel[i];
            scopeBufferWriteIndex = (scopeBufferWriteIndex + 1) % scopeBufferSize;
        }
    }
}

void PhotoSynthAudioProcessor::getScopeData(float* destination, int numSamples)
{
    const juce::ScopedLock sl(scopeLock);
    int readIdx = scopeBufferWriteIndex;
    for (int i = 0; i < numSamples; ++i) {
        int idx = (readIdx + i) % scopeBufferSize;
        destination[i] = scopeBuffer[idx];
    }
}

juce::AudioProcessorEditor* PhotoSynthAudioProcessor::createEditor()
{
    return new PhotoSynthAudioEditor (*this);
}

void PhotoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PhotoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName (apvts.state.getType())) {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
