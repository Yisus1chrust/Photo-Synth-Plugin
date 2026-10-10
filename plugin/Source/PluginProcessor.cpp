/*
  ==============================================================================
    PluginProcessor.cpp
    Modular Sound Technologies - Paint a Synth (Photo Synth)
    JUCE 8 Hybrid WebView Plugin Architecture
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PhotoSynthAudioProcessor::PhotoSynthAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Register custom sound and 16 polyphonic voice instances
    synth.addSound (new PhotoSynthSound());
    for (int i = 0; i < 16; ++i) {
        synth.addVoice (new SynthVoice());
    }

    // Default starting presets matching the web UI
    imageSlotA = photosynth::PresetFactory::createSynthwaveSunset();
    imageSlotB = photosynth::PresetFactory::createObsidianCrypt();
    analysisSlotA = ImageAnalyzer::analyzeImage (imageSlotA);
    analysisSlotB = ImageAnalyzer::analyzeImage (imageSlotB);
    slotAName = "Synthwave Sunset";
    slotBName = "Obsidian Crypt";

    updateMorphState();
    updateBlendedImage();
    applyAnalysisToParameters (currentAnalysis);
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Core Synthesis Parameters
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff Frequency", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2500.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 10.0f, 0.05f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack Time", juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f), 0.015f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "Decay Time", juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.35f), 0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sustain", 1 }, "Sustain Level", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release Time", juce::NormalisableRange<float> (0.01f, 8.0f, 0.01f, 0.35f), 0.4f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Saturation Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Master Volume", juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -3.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "morph", 1 }, "A/B Morph", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "archetype", 1 }, "Synth Archetype", juce::NormalisableRange<float> (0.0f, 2.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bpm", 1 }, "Tempo BPM", juce::NormalisableRange<float> (20.0f, 300.0f, 1.0f), 120.0f));

    // 2. Timbre DNA Parameters (Active Visual Synthesizer Controls)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "color", 1 }, "Color (Hue)", juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), 88.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "brightness", 1 }, "Brightness", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 45.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "saturation", 1 }, "Saturation", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 83.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "complexity", 1 }, "Complexity", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 52.0f));

    // 3. Modulation & Effects Parameters
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoRate", 1 }, "LFO Rate", juce::NormalisableRange<float> (0.1f, 20.0f, 0.1f), 2.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoDepth", 1 }, "LFO Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "limiter", 1 }, "Limiter Toggle", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "threshold", 1 }, "Threshold", juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), -3.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "delayEnabled", 1 }, "Delay Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "delayTime", 1 }, "Delay Time", juce::NormalisableRange<float> (10.0f, 1500.0f, 1.0f), 250.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "delayFeedback", 1 }, "Delay Feedback", juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "delayMix", 1 }, "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "reverbEnabled", 1 }, "Reverb Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverbSize", 1 }, "Reverb Size", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverbDecay", 1 }, "Reverb Decay", juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 1.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "reverbMix", 1 }, "Reverb Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.30f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "chorusEnabled", 1 }, "Chorus Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "chorusRate", 1 }, "Chorus Rate", juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "chorusDepth", 1 }, "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "chorusMix", 1 }, "Chorus Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "phaserEnabled", 1 }, "Phaser Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "phaserRate", 1 }, "Phaser Rate", juce::NormalisableRange<float> (0.1f, 8.0f, 0.1f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "phaserDepth", 1 }, "Phaser Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "phaserFeedback", 1 }, "Phaser Feedback", juce::NormalisableRange<float> (0.0f, 0.8f, 0.01f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "phaserMix", 1 }, "Phaser Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "flangerEnabled", 1 }, "Flanger Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "flangerRate", 1 }, "Flanger Rate", juce::NormalisableRange<float> (0.05f, 5.0f, 0.05f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "flangerDepth", 1 }, "Flanger Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "flangerMix", 1 }, "Flanger Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "distortionEnabled", 1 }, "Distortion Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "distortionDrive", 1 }, "Distortion Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "distortionMix", 1 }, "Distortion Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    return { params.begin(), params.end() };
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PhotoSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // A software instrument must NOT accept audio inputs!
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    // Output must be stereo or mono
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}
#endif

void PhotoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    updateMorphState();

    VoiceParams p;
    p.cutoff    = apvts.getRawParameterValue ("cutoff")->load();
    p.resonance = apvts.getRawParameterValue ("resonance")->load();
    p.lfoRate   = apvts.getRawParameterValue ("lfoRate")->load();
    p.lfoDepth  = apvts.getRawParameterValue ("lfoDepth")->load();
    p.attack    = apvts.getRawParameterValue ("attack")->load();
    p.decay     = apvts.getRawParameterValue ("decay")->load();
    p.sustain   = apvts.getRawParameterValue ("sustain")->load();
    p.release   = apvts.getRawParameterValue ("release")->load();
    p.drive     = apvts.getRawParameterValue ("drive")->load();

    if (auto* cp = apvts.getRawParameterValue ("color"))
        p.color = cp->load();
    else
        p.color = currentAnalysis.timbreDna * 360.0f;

    if (auto* bp = apvts.getRawParameterValue ("brightness"))
        p.brightness = bp->load();
    else
        p.brightness = currentAnalysis.brightness * 100.0f;

    if (auto* sp = apvts.getRawParameterValue ("saturation"))
        p.saturation = sp->load();
    else
        p.saturation = currentAnalysis.saturation * 100.0f;

    if (auto* cxp = apvts.getRawParameterValue ("complexity"))
        p.complexity = cxp->load();
    else
        p.complexity = currentAnalysis.complexity * 100.0f;

    float archVal = apvts.getRawParameterValue ("archetype")->load();
    int archInt = (int) std::round (archVal);
    if (archInt == 1) p.archetype = SynthArchetype::DigitalFmBells;
    else if (archInt == 2) p.archetype = SynthArchetype::HybridWavetable;
    else if (archInt == 0) p.archetype = SynthArchetype::AnalogBrass;
    else p.archetype = currentAnalysis.archetype;
    p.era = currentAnalysis.temporalEra.era;

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i))) {
            voice->prepare (sampleRate, samplesPerBlock);
            voice->updatePatch (p);
        }
    }

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    effectsChain.prepare (spec);
    masterLimiter.prepare (spec);
}

void PhotoSynthAudioProcessor::releaseResources()
{
    effectsChain.reset();
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 1. Explicitly clear all output channels so no host residual audio passes through
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    buffer.clear();

    // 2. Sync DAW BPM Transport
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            if (auto hostBpm = pos->getBpm()) {
                currentBpm = juce::jlimit (20.0f, 300.0f, (float)*hostBpm);
            }
        }
    }

    // 3. Pre-bind live parameters to all polyphonic voices BEFORE processing MIDI or audio
    updateMorphState();

    VoiceParams p;
    p.cutoff    = apvts.getRawParameterValue ("cutoff")->load();
    p.resonance = apvts.getRawParameterValue ("resonance")->load();
    p.lfoRate   = apvts.getRawParameterValue ("lfoRate")->load();
    p.lfoDepth  = apvts.getRawParameterValue ("lfoDepth")->load();
    p.attack    = apvts.getRawParameterValue ("attack")->load();
    p.decay     = apvts.getRawParameterValue ("decay")->load();
    p.sustain   = apvts.getRawParameterValue ("sustain")->load();
    p.release   = apvts.getRawParameterValue ("release")->load();
    p.drive     = apvts.getRawParameterValue ("drive")->load();

    if (auto* cp = apvts.getRawParameterValue ("color"))
        p.color = cp->load();
    else
        p.color = currentAnalysis.timbreDna * 360.0f;

    if (auto* bp = apvts.getRawParameterValue ("brightness"))
        p.brightness = bp->load();
    else
        p.brightness = currentAnalysis.brightness * 100.0f;

    if (auto* sp = apvts.getRawParameterValue ("saturation"))
        p.saturation = sp->load();
    else
        p.saturation = currentAnalysis.saturation * 100.0f;

    if (auto* cxp = apvts.getRawParameterValue ("complexity"))
        p.complexity = cxp->load();
    else
        p.complexity = currentAnalysis.complexity * 100.0f;

    float archVal = apvts.getRawParameterValue ("archetype")->load();
    int archInt = (int) std::round (archVal);
    if (archInt == 1) p.archetype = SynthArchetype::DigitalFmBells;
    else if (archInt == 2) p.archetype = SynthArchetype::HybridWavetable;
    else if (archInt == 0) p.archetype = SynthArchetype::AnalogBrass;
    else p.archetype = currentAnalysis.archetype;
    p.era = currentAnalysis.temporalEra.era;

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice (i))) {
            voice->updatePatch (p);
        }
    }

    // 4. Thread-safe merge of virtual UI keyboard events with incoming DAW MIDI
    {
        const juce::ScopedLock sl (midiLock);
        if (!internalMidiBuffer.isEmpty()) {
            for (const auto metadata : internalMidiBuffer) {
                const int samplePos = juce::jlimit (0, juce::jmax (0, buffer.getNumSamples() - 1), metadata.samplePosition);
                midiMessages.addEvent (metadata.getMessage(), samplePos);
            }
            internalMidiBuffer.clear();
        }
    }

    // 5. Render Polyphonic Synthesizer
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // 6. Effects Chain & Master Gain
    photosynth::EffectsParameters fxParams;
    fxParams.delayEnabled   = apvts.getRawParameterValue ("delayEnabled")->load() > 0.5f;
    fxParams.delayTimeMs    = apvts.getRawParameterValue ("delayTime")->load();
    fxParams.delayFeedback  = apvts.getRawParameterValue ("delayFeedback")->load();
    fxParams.delayMix       = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("delayMix")->load() * (0.6f + currentAnalysis.delayMix * 0.8f));

    fxParams.reverbEnabled  = apvts.getRawParameterValue ("reverbEnabled")->load() > 0.5f;
    fxParams.reverbRoomSize = apvts.getRawParameterValue ("reverbSize")->load();
    fxParams.reverbDecaySec = apvts.getRawParameterValue ("reverbDecay")->load();
    fxParams.reverbMix      = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue ("reverbMix")->load() * (0.6f + currentAnalysis.reverbMix * 0.8f));

    fxParams.chorusEnabled  = apvts.getRawParameterValue ("chorusEnabled")->load() > 0.5f;
    fxParams.chorusRateHz   = apvts.getRawParameterValue ("chorusRate")->load();
    fxParams.chorusDepth    = apvts.getRawParameterValue ("chorusDepth")->load();
    fxParams.chorusMix      = apvts.getRawParameterValue ("chorusMix")->load();

    effectsChain.updateParameters (fxParams);
    effectsChain.processBlock (buffer);

    float volDb = apvts.getRawParameterValue ("volume")->load();
    float masterGain = juce::Decibels::decibelsToGain (volDb, -60.0f) * 0.95f;
    buffer.applyGain (masterGain);

    // 7. Oscilloscope FIFO Capture
    {
        const juce::ScopedLock sl (scopeLock);
        const float* leftChannel = buffer.getReadPointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            scopeBuffer[scopeBufferWriteIndex] = leftChannel[i];
            scopeBufferWriteIndex = (scopeBufferWriteIndex + 1) % scopeBufferSize;
        }
    }
}

// 8. Explicit State Serialization (APVTS Binary XML)
void PhotoSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr) {
        xml->setAttribute ("slotAName", slotAName);
        xml->setAttribute ("slotBName", slotBName);
        xml->setAttribute ("currentBpm", (double) currentBpm);

        copyXmlToBinary (*xml, destData);
    }
}

void PhotoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName (apvts.state.getType())) {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        } else if (auto* child = xmlState->getChildByName (apvts.state.getType().toString())) {
            apvts.replaceState (juce::ValueTree::fromXml (*child));
        } else {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }

        if (xmlState->hasAttribute ("slotAName"))
            slotAName = xmlState->getStringAttribute ("slotAName");
        if (xmlState->hasAttribute ("slotBName"))
            slotBName = xmlState->getStringAttribute ("slotBName");
        if (xmlState->hasAttribute ("currentBpm"))
            currentBpm = (float) xmlState->getDoubleAttribute ("currentBpm", 120.0);

        updateMorphState();
        updateBlendedImage();
    }
}

void PhotoSynthAudioProcessor::noteOn (int midiNoteNumber, float velocity, int midiChannel)
{
    const int ch = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    const juce::ScopedLock sl (midiLock);
    internalMidiBuffer.addEvent (juce::MidiMessage::noteOn (ch, midiNoteNumber, velocity), 0);
}

void PhotoSynthAudioProcessor::noteOff (int midiNoteNumber, int midiChannel)
{
    const int ch = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    const juce::ScopedLock sl (midiLock);
    internalMidiBuffer.addEvent (juce::MidiMessage::noteOff (ch, midiNoteNumber), 0);
}

void PhotoSynthAudioProcessor::allNotesOff (int midiChannel)
{
    const juce::ScopedLock sl (midiLock);
    if (midiChannel >= 1 && midiChannel <= 16) {
        internalMidiBuffer.addEvent (juce::MidiMessage::allNotesOff (midiChannel), 0);
        synth.allNotesOff (midiChannel, false);
    } else {
        for (int ch = 1; ch <= 16; ++ch) {
            internalMidiBuffer.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
        }
        synth.allNotesOff (0, false);
    }
}

void PhotoSynthAudioProcessor::getScopeData (float* destination, int numSamples)
{
    const juce::ScopedLock sl (scopeLock);
    int readIdx = (scopeBufferWriteIndex - numSamples + scopeBufferSize) % scopeBufferSize;
    for (int i = 0; i < numSamples; ++i) {
        destination[i] = scopeBuffer[(readIdx + i) % scopeBufferSize];
    }
}

void PhotoSynthAudioProcessor::loadSlotAImage (const juce::Image& img, const juce::String& name)
{
    if (img.isValid()) {
        imageSlotA = img;
        analysisSlotA = ImageAnalyzer::analyzeImage (img);
        if (name.isNotEmpty()) slotAName = name;
        updateMorphState();
        updateBlendedImage();
    }
}

void PhotoSynthAudioProcessor::loadSlotBImage (const juce::Image& img, const juce::String& name)
{
    if (img.isValid()) {
        imageSlotB = img;
        analysisSlotB = ImageAnalyzer::analyzeImage (img);
        if (name.isNotEmpty()) slotBName = name;
        updateMorphState();
        updateBlendedImage();
    }
}

void PhotoSynthAudioProcessor::loadSlotAPreset (int presetIndex)
{
    switch (presetIndex) {
        case 0: loadSlotAImage (photosynth::PresetFactory::createSynthwaveSunset(), "Synthwave Sunset"); break;
        case 1: loadSlotAImage (photosynth::PresetFactory::createObsidianCrypt(), "Obsidian Crypt"); break;
        case 2: loadSlotAImage (photosynth::PresetFactory::createBioluminescentCoral(), "Bioluminescent Coral"); break;
        case 3: loadSlotAImage (photosynth::PresetFactory::createDesertDunes(), "Desert Dunes"); break;
        case 4: loadSlotAImage (photosynth::PresetFactory::createCyberpunkAlley(), "Cyberpunk Alley"); break;
        default: break;
    }
}

void PhotoSynthAudioProcessor::loadSlotBPreset (int presetIndex)
{
    switch (presetIndex) {
        case 0: loadSlotBImage (photosynth::PresetFactory::createSynthwaveSunset(), "Synthwave Sunset"); break;
        case 1: loadSlotBImage (photosynth::PresetFactory::createObsidianCrypt(), "Obsidian Crypt"); break;
        case 2: loadSlotBImage (photosynth::PresetFactory::createBioluminescentCoral(), "Bioluminescent Coral"); break;
        case 3: loadSlotBImage (photosynth::PresetFactory::createDesertDunes(), "Desert Dunes"); break;
        case 4: loadSlotBImage (photosynth::PresetFactory::createCyberpunkAlley(), "Cyberpunk Alley"); break;
        default: break;
    }
}

juce::Image PhotoSynthAudioProcessor::getBlendedImage()
{
    return blendedImage.isValid() ? blendedImage : imageSlotA;
}

juce::String PhotoSynthAudioProcessor::getImageSlotAAsDataUrl() const
{
    if (!imageSlotA.isValid()) return {};
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat pngFormat;
    pngFormat.writeImageToStream (imageSlotA, mos);
    return "data:image/png;base64," + juce::Base64::toBase64 (mos.getData(), mos.getDataSize());
}

juce::String PhotoSynthAudioProcessor::getImageSlotBAsDataUrl() const
{
    if (!imageSlotB.isValid()) return {};
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat pngFormat;
    pngFormat.writeImageToStream (imageSlotB, mos);
    return "data:image/png;base64," + juce::Base64::toBase64 (mos.getData(), mos.getDataSize());
}

void PhotoSynthAudioProcessor::syncSlotImageFromFrontend (const juce::String& slot, const juce::String& dataUrl, const juce::String& name)
{
    int commaIdx = dataUrl.indexOf (",");
    if (commaIdx >= 0) {
        juce::String base64Data = dataUrl.substring (commaIdx + 1);
        juce::MemoryOutputStream mos;
        if (juce::Base64::convertFromBase64 (mos, base64Data)) {
            juce::MemoryInputStream mis (mos.getData(), mos.getDataSize(), false);
            juce::PNGImageFormat pngFormat;
            juce::Image decodedImg = pngFormat.decodeImage (mis);
            if (!decodedImg.isValid()) {
                juce::JPEGImageFormat jpegFormat;
                mis.setPosition (0);
                decodedImg = jpegFormat.decodeImage (mis);
            }
            if (decodedImg.isValid()) {
                if (slot == "A") loadSlotAImage (decodedImg, name);
                else loadSlotBImage (decodedImg, name);
            }
        }
    }
}

void PhotoSynthAudioProcessor::updateMorphState()
{
    float morphT = apvts.getRawParameterValue ("morph")->load();
    currentAnalysis = ImageAnalyzer::interpolate (analysisSlotA, analysisSlotB, morphT);
}

void PhotoSynthAudioProcessor::updateBlendedImage()
{
    float morphT = apvts.getRawParameterValue ("morph")->load();
    if (morphT <= 0.01f && imageSlotA.isValid()) {
        blendedImage = imageSlotA;
    } else if (morphT >= 0.99f && imageSlotB.isValid()) {
        blendedImage = imageSlotB;
    } else if (imageSlotA.isValid() && imageSlotB.isValid()) {
        blendedImage = ImageAnalyzer::createBlendedPreview (imageSlotA, imageSlotB, morphT);
    }
}

void PhotoSynthAudioProcessor::applyAnalysisToParameters (const ImageAnalysisResult& analysis)
{
    if (auto* p = apvts.getParameter ("cutoff")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.cutoff));
    if (auto* p = apvts.getParameter ("resonance")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.resonance));
    if (auto* p = apvts.getParameter ("attack")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.attack));
    if (auto* p = apvts.getParameter ("decay")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.decay));
    if (auto* p = apvts.getParameter ("sustain")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.sustain));
    if (auto* p = apvts.getParameter ("release")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.release));
    if (auto* p = apvts.getParameter ("drive")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.drive));
    if (auto* p = apvts.getParameter ("color")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.timbreDna * 360.0f));
    if (auto* p = apvts.getParameter ("brightness")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.brightness * 100.0f));
    if (auto* p = apvts.getParameter ("saturation")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.saturation * 100.0f));
    if (auto* p = apvts.getParameter ("complexity")) p->setValueNotifyingHost (p->convertTo0to1 (analysis.complexity * 100.0f));
}

void PhotoSynthAudioProcessor::updateParametersFromAnalysis()
{
    updateMorphState();
    applyAnalysisToParameters (currentAnalysis);
}

void PhotoSynthAudioProcessor::setBpm (float newBpm)
{
    currentBpm = juce::jlimit (20.0f, 300.0f, newBpm);
}

juce::AudioProcessorEditor* PhotoSynthAudioProcessor::createEditor()
{
    return new PhotoSynthAudioEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
