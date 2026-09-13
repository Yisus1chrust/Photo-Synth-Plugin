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

    // Initialize Slot A and Slot B with built-in procedural presets
    imageSlotA = photosynth::PresetFactory::createSynthwaveSunset();
    imageSlotB = photosynth::PresetFactory::createObsidianCrypt();
    analysisSlotA = ImageAnalyzer::analyzeImage(imageSlotA);
    analysisSlotB = ImageAnalyzer::analyzeImage(imageSlotB);
    slotAName = "Synthwave Sunset";
    slotBName = "Obsidian Crypt";

    updateMorphState();
    updateBlendedImage();
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Filter Cutoff & Resonance
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff Frequency", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2500.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 10.0f, 0.05f), 1.0f));

    // LFO Modulation
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoRate", 1 }, "LFO Rate", juce::NormalisableRange<float> (0.1f, 20.0f, 0.1f), 2.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoDepth", 1 }, "LFO Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    // Master Volume & Envelopes
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Master Volume", juce::NormalisableRange<float> (-60.0f, 6.0f, 0.1f), -3.0f));

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

    // Effects: Delay
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "delayEnabled", 1 }, "Delay Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delayTime", 1 }, "Delay Time", juce::NormalisableRange<float> (10.0f, 1500.0f, 1.0f), 250.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delayFeedback", 1 }, "Delay Feedback", juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "delayMix", 1 }, "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    // Effects: Reverb
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "reverbEnabled", 1 }, "Reverb Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverbSize", 1 }, "Reverb Size", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.65f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverbDecay", 1 }, "Reverb Decay", juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 1.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reverbMix", 1 }, "Reverb Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.30f));

    // Effects: Chorus
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "chorusEnabled", 1 }, "Chorus Enabled", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorusRate", 1 }, "Chorus Rate", juce::NormalisableRange<float> (0.1f, 10.0f, 0.1f), 1.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorusDepth", 1 }, "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "chorusMix", 1 }, "Chorus Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    // Effects: Phaser
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "phaserEnabled", 1 }, "Phaser Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "phaserRate", 1 }, "Phaser Rate", juce::NormalisableRange<float> (0.1f, 8.0f, 0.1f), 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "phaserDepth", 1 }, "Phaser Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "phaserFeedback", 1 }, "Phaser Feedback", juce::NormalisableRange<float> (0.0f, 0.8f, 0.01f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "phaserMix", 1 }, "Phaser Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // Effects: Flanger
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "flangerEnabled", 1 }, "Flanger Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flangerRate", 1 }, "Flanger Rate", juce::NormalisableRange<float> (0.05f, 5.0f, 0.05f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flangerDepth", 1 }, "Flanger Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "flangerMix", 1 }, "Flanger Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    // Effects: Distortion
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "distortionEnabled", 1 }, "Distortion Enabled", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "distortionDrive", 1 }, "Distortion Drive", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "distortionMix", 1 }, "Distortion Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

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
    effectsChain.prepare(spec);

    masterLimiter.setThreshold(*apvts.getRawParameterValue("threshold"));
    masterLimiter.prepare(spec);
}

void PhotoSynthAudioProcessor::releaseResources()
{
    effectsChain.reset();
}

void PhotoSynthAudioProcessor::loadSlotAImage(const juce::Image& img, const juce::String& name)
{
    imageSlotA = img;
    slotAName = name.isNotEmpty() ? name : "Image A";
    analysisSlotA = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
    updateBlendedImage();
}

void PhotoSynthAudioProcessor::loadSlotBImage(const juce::Image& img, const juce::String& name)
{
    imageSlotB = img;
    slotBName = name.isNotEmpty() ? name : "Image B";
    analysisSlotB = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
    updateBlendedImage();
}

void PhotoSynthAudioProcessor::loadSlotAPreset(int presetIndex)
{
    imageSlotA = photosynth::PresetFactory::getPresetImage(presetIndex);
    slotAName = photosynth::PresetFactory::getPresetName(presetIndex);
    analysisSlotA = ImageAnalyzer::analyzeImage(imageSlotA);
    updateMorphState();
    updateBlendedImage();
}

void PhotoSynthAudioProcessor::loadSlotBPreset(int presetIndex)
{
    imageSlotB = photosynth::PresetFactory::getPresetImage(presetIndex);
    slotBName = photosynth::PresetFactory::getPresetName(presetIndex);
    analysisSlotB = ImageAnalyzer::analyzeImage(imageSlotB);
    updateMorphState();
    updateBlendedImage();
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

void PhotoSynthAudioProcessor::updateBlendedImage()
{
    float morph = *apvts.getRawParameterValue("morph");
    int w = 400;
    int h = 300;

    blendedImage = juce::Image(juce::Image::RGB, w, h, true);
    juce::Graphics g(blendedImage);

    if (imageSlotA.isValid()) {
        g.setOpacity(1.0f);
        g.drawImage(imageSlotA, 0, 0, w, h, 0, 0, imageSlotA.getWidth(), imageSlotA.getHeight());
    }

    if (imageSlotB.isValid() && morph > 0.001f) {
        g.setOpacity(juce::jlimit(0.0f, 1.0f, morph));
        g.drawImage(imageSlotB, 0, 0, w, h, 0, 0, imageSlotB.getWidth(), imageSlotB.getHeight());
    }
}

juce::Image PhotoSynthAudioProcessor::getBlendedImage()
{
    updateBlendedImage();
    return blendedImage;
}

void PhotoSynthAudioProcessor::noteOn(int midiNoteNumber, float velocity)
{
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::noteOn(1, midiNoteNumber, velocity), 0);
}

void PhotoSynthAudioProcessor::noteOff(int midiNoteNumber)
{
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::noteOff(1, midiNoteNumber), 0);
}

void PhotoSynthAudioProcessor::allNotesOff()
{
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Merge internal UI MIDI events
    {
        const juce::ScopedLock sl(midiLock);
        if (!internalMidiBuffer.isEmpty()) {
            midiMessages.addEvents(internalMidiBuffer, 0, buffer.getNumSamples(), 0);
            internalMidiBuffer.clear();
        }
    }

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

    // Update and process Audio Effects Chain
    photosynth::EffectsParameters fxParams;
    fxParams.delayEnabled       = *apvts.getRawParameterValue("delayEnabled") > 0.5f;
    fxParams.delayTimeMs        = *apvts.getRawParameterValue("delayTime");
    fxParams.delayFeedback      = *apvts.getRawParameterValue("delayFeedback");
    fxParams.delayMix           = *apvts.getRawParameterValue("delayMix");

    fxParams.reverbEnabled      = *apvts.getRawParameterValue("reverbEnabled") > 0.5f;
    fxParams.reverbRoomSize     = *apvts.getRawParameterValue("reverbSize");
    fxParams.reverbDecaySec     = *apvts.getRawParameterValue("reverbDecay");
    fxParams.reverbMix          = *apvts.getRawParameterValue("reverbMix");

    fxParams.chorusEnabled      = *apvts.getRawParameterValue("chorusEnabled") > 0.5f;
    fxParams.chorusRateHz       = *apvts.getRawParameterValue("chorusRate");
    fxParams.chorusDepth        = *apvts.getRawParameterValue("chorusDepth");
    fxParams.chorusMix          = *apvts.getRawParameterValue("chorusMix");

    fxParams.phaserEnabled      = *apvts.getRawParameterValue("phaserEnabled") > 0.5f;
    fxParams.phaserRateHz       = *apvts.getRawParameterValue("phaserRate");
    fxParams.phaserDepth        = *apvts.getRawParameterValue("phaserDepth");
    fxParams.phaserFeedback     = *apvts.getRawParameterValue("phaserFeedback");
    fxParams.phaserMix          = *apvts.getRawParameterValue("phaserMix");

    fxParams.flangerEnabled     = *apvts.getRawParameterValue("flangerEnabled") > 0.5f;
    fxParams.flangerRateHz      = *apvts.getRawParameterValue("flangerRate");
    fxParams.flangerDepth       = *apvts.getRawParameterValue("flangerDepth");
    fxParams.flangerMix         = *apvts.getRawParameterValue("flangerMix");

    fxParams.distortionEnabled  = *apvts.getRawParameterValue("distortionEnabled") > 0.5f;
    fxParams.distortionDrive    = *apvts.getRawParameterValue("distortionDrive");
    fxParams.distortionMix      = *apvts.getRawParameterValue("distortionMix");

    effectsChain.updateParameters(fxParams);
    effectsChain.processBlock(buffer);

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

void PhotoSynthAudioProcessor::saveCurrentPreset(const juce::String& name, const juce::String&)
{
    if (name.isNotEmpty() && !userPresetNames.contains(name)) {
        userPresetNames.add(name);
    }
}

juce::StringArray PhotoSynthAudioProcessor::getUserPresetNames() const
{
    return userPresetNames;
}

void PhotoSynthAudioProcessor::loadUserPreset(const juce::String&)
{
    // Preset recall from saved state
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
