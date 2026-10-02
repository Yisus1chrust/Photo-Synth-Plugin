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
    applyAnalysisToParameters(currentAnalysis);

    loadUserPresetsFromDisk();
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Tempo BPM
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bpm", 1 }, "Tempo BPM", juce::NormalisableRange<float> (20.0f, 300.0f, 1.0f), 120.0f));

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

    // Synth Archetype (0: AnalogBrass, 1: DigitalFmBells, 2: HybridWavetable)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "archetype", 1 }, "Synth Archetype", juce::NormalisableRange<float> (0.0f, 2.0f, 1.0f), 0.0f));

    // Timbre DNA & Visual Sound Shaping Parameters (Color, Brightness, Saturation, Complexity)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "color", 1 }, "Color (Hue)", juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), 88.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "brightness", 1 }, "Brightness", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 45.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "saturation", 1 }, "Saturation", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 83.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "complexity", 1 }, "Complexity", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 52.0f));

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

void PhotoSynthAudioProcessor::setBpm(float newBpm)
{
    currentBpm = juce::jlimit(20.0f, 300.0f, newBpm);
    if (auto* param = apvts.getParameter("bpm")) {
        param->setValueNotifyingHost(param->convertTo0to1(currentBpm));
    }
}

void PhotoSynthAudioProcessor::applyAnalysisToParameters(const ImageAnalysisResult& analysis)
{
    auto setParam = [this](const juce::String& paramId, float value) {
        if (auto* param = apvts.getParameter(paramId)) {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    setParam("cutoff",     analysis.cutoff);
    setParam("resonance",  analysis.resonance);
    setParam("lfoRate",    analysis.lfoRate);
    setParam("lfoDepth",   analysis.lfoDepth);
    setParam("attack",     analysis.attack);
    setParam("decay",      analysis.decay);
    setParam("sustain",    analysis.sustain);
    setParam("release",    analysis.release);
    setParam("drive",      analysis.drive);
    setParam("delayMix",   analysis.delayMix);
    setParam("reverbMix",  analysis.reverbMix);
    setParam("archetype",  (float)analysis.archetype);

    setParam("color",      analysis.timbreDna * 360.0f);
    setParam("brightness", analysis.brightness * 100.0f);
    setParam("saturation", analysis.saturation * 100.0f);
    setParam("complexity", analysis.complexity * 100.0f);
}

void PhotoSynthAudioProcessor::updateParametersFromAnalysis()
{
    applyAnalysisToParameters(currentAnalysis);
}

void PhotoSynthAudioProcessor::loadSlotAImage(const juce::Image& img, const juce::String& name)
{
    imageSlotA = img;
    slotAName = name.isNotEmpty() ? name : "Image A";
    analysisSlotA = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
    updateBlendedImage();
    updateParametersFromAnalysis();
}

void PhotoSynthAudioProcessor::loadSlotBImage(const juce::Image& img, const juce::String& name)
{
    imageSlotB = img;
    slotBName = name.isNotEmpty() ? name : "Image B";
    analysisSlotB = ImageAnalyzer::analyzeImage(img);
    updateMorphState();
    updateBlendedImage();
    updateParametersFromAnalysis();
}

void PhotoSynthAudioProcessor::loadSlotAPreset(int presetIndex)
{
    imageSlotA = photosynth::PresetFactory::getPresetImage(presetIndex);
    slotAName = photosynth::PresetFactory::getPresetName(presetIndex);
    analysisSlotA = ImageAnalyzer::analyzeImage(imageSlotA);
    updateMorphState();
    updateBlendedImage();
    updateParametersFromAnalysis();
}

void PhotoSynthAudioProcessor::loadSlotBPreset(int presetIndex)
{
    imageSlotB = photosynth::PresetFactory::getPresetImage(presetIndex);
    slotBName = photosynth::PresetFactory::getPresetName(presetIndex);
    analysisSlotB = ImageAnalyzer::analyzeImage(imageSlotB);
    updateMorphState();
    updateBlendedImage();
    updateParametersFromAnalysis();
}

void PhotoSynthAudioProcessor::updateMorphState()
{
    float morph = *apvts.getRawParameterValue("morph");

    // Linearly interpolate metrics between Slot A and Slot B
    currentAnalysis.brightness = (1.0f - morph) * analysisSlotA.brightness + morph * analysisSlotB.brightness;
    currentAnalysis.saturation = (1.0f - morph) * analysisSlotA.saturation + morph * analysisSlotB.saturation;
    currentAnalysis.complexity = (1.0f - morph) * analysisSlotA.complexity + morph * analysisSlotB.complexity;
    currentAnalysis.timbreDna  = (1.0f - morph) * analysisSlotA.timbreDna  + morph * analysisSlotB.timbreDna;

    // Linearly interpolate synthesized audio parameters
    currentAnalysis.cutoff     = (1.0f - morph) * analysisSlotA.cutoff     + morph * analysisSlotB.cutoff;
    currentAnalysis.resonance  = (1.0f - morph) * analysisSlotA.resonance  + morph * analysisSlotB.resonance;
    currentAnalysis.attack     = (1.0f - morph) * analysisSlotA.attack     + morph * analysisSlotB.attack;
    currentAnalysis.decay      = (1.0f - morph) * analysisSlotA.decay      + morph * analysisSlotB.decay;
    currentAnalysis.sustain    = (1.0f - morph) * analysisSlotA.sustain    + morph * analysisSlotB.sustain;
    currentAnalysis.release    = (1.0f - morph) * analysisSlotA.release    + morph * analysisSlotB.release;
    currentAnalysis.lfoRate    = (1.0f - morph) * analysisSlotA.lfoRate    + morph * analysisSlotB.lfoRate;
    currentAnalysis.lfoDepth   = (1.0f - morph) * analysisSlotA.lfoDepth   + morph * analysisSlotB.lfoDepth;
    currentAnalysis.drive      = (1.0f - morph) * analysisSlotA.drive      + morph * analysisSlotB.drive;
    currentAnalysis.delayMix   = (1.0f - morph) * analysisSlotA.delayMix   + morph * analysisSlotB.delayMix;
    currentAnalysis.reverbMix  = (1.0f - morph) * analysisSlotA.reverbMix  + morph * analysisSlotB.reverbMix;

    currentAnalysis.archetype = (morph < 0.5f) ? analysisSlotA.archetype : analysisSlotB.archetype;
    currentAnalysis.temporalEra = (morph < 0.5f) ? analysisSlotA.temporalEra : analysisSlotB.temporalEra;
}

void PhotoSynthAudioProcessor::updateBlendedImage()
{
    float morph = juce::jlimit(0.0f, 1.0f, (float)*apvts.getRawParameterValue("morph"));
    int w = 500;
    int h = 380;

    blendedImage = juce::Image(juce::Image::RGB, w, h, true);
    juce::Graphics g(blendedImage);

    if (imageSlotA.isValid() && imageSlotB.isValid()) {
        g.setOpacity(1.0f - morph);
        g.drawImage(imageSlotA, 0, 0, w, h, 0, 0, imageSlotA.getWidth(), imageSlotA.getHeight());
        g.setOpacity(morph);
        g.drawImage(imageSlotB, 0, 0, w, h, 0, 0, imageSlotB.getWidth(), imageSlotB.getHeight());
    } else if (imageSlotA.isValid()) {
        g.setOpacity(1.0f);
        g.drawImage(imageSlotA, 0, 0, w, h, 0, 0, imageSlotA.getWidth(), imageSlotA.getHeight());
    } else if (imageSlotB.isValid()) {
        g.setOpacity(1.0f);
        g.drawImage(imageSlotB, 0, 0, w, h, 0, 0, imageSlotB.getWidth(), imageSlotB.getHeight());
    }
}

juce::Image PhotoSynthAudioProcessor::getBlendedImage()
{
    updateBlendedImage();
    return blendedImage;
}

juce::String PhotoSynthAudioProcessor::getImageSlotAAsDataUrl() const
{
    if (!imageSlotA.isValid()) return {};
    juce::Image thumb = imageSlotA;
    const int maxDim = 512;
    if (thumb.getWidth() > maxDim || thumb.getHeight() > maxDim) {
        int tw = thumb.getWidth(), th = thumb.getHeight();
        if (tw > th) { th = std::max(1, (th * maxDim) / tw); tw = maxDim; }
        else { tw = std::max(1, (tw * maxDim) / th); th = maxDim; }
        thumb = thumb.rescaled(tw, th, juce::Graphics::mediumResamplingQuality);
    }
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat png;
    if (png.writeImageToStream(thumb, mos) && mos.getDataSize() > 0) {
        return "data:image/png;base64," + juce::Base64::toBase64(mos.getData(), mos.getDataSize());
    }
    return {};
}

juce::String PhotoSynthAudioProcessor::getImageSlotBAsDataUrl() const
{
    if (!imageSlotB.isValid()) return {};
    juce::Image thumb = imageSlotB;
    const int maxDim = 512;
    if (thumb.getWidth() > maxDim || thumb.getHeight() > maxDim) {
        int tw = thumb.getWidth(), th = thumb.getHeight();
        if (tw > th) { th = std::max(1, (th * maxDim) / tw); tw = maxDim; }
        else { tw = std::max(1, (tw * maxDim) / th); th = maxDim; }
        thumb = thumb.rescaled(tw, th, juce::Graphics::mediumResamplingQuality);
    }
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat png;
    if (png.writeImageToStream(thumb, mos) && mos.getDataSize() > 0) {
        return "data:image/png;base64," + juce::Base64::toBase64(mos.getData(), mos.getDataSize());
    }
    return {};
}

void PhotoSynthAudioProcessor::syncSlotImageFromFrontend(const juce::String& slot, const juce::String& dataUrl, const juce::String& name)
{
    if (dataUrl.isEmpty()) return;

    juce::String b64 = dataUrl;
    int commaIdx = b64.indexOfChar(',');
    if (commaIdx >= 0) {
        b64 = b64.substring(commaIdx + 1);
    }

    juce::MemoryOutputStream mos;
    juce::Base64::convertFromBase64(mos, b64);
    auto img = juce::ImageFileFormat::loadFrom(mos.getData(), mos.getDataSize());
    if (img.isValid()) {
        if (slot.equalsIgnoreCase("B")) {
            imageSlotB = img;
            slotBName = name.isNotEmpty() ? name : "Image B";
            analysisSlotB = ImageAnalyzer::analyzeImage(img);
        } else {
            imageSlotA = img;
            slotAName = name.isNotEmpty() ? name : "Image A";
            analysisSlotA = ImageAnalyzer::analyzeImage(img);
        }
        updateMorphState();
        updateBlendedImage();
    }
}

void PhotoSynthAudioProcessor::noteOn(int midiNoteNumber, float velocity)
{
    float vel = juce::jlimit(0.05f, 1.0f, velocity);
    synth.noteOn(1, midiNoteNumber, vel);
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::noteOn(1, midiNoteNumber, vel), 0);
}

void PhotoSynthAudioProcessor::noteOff(int midiNoteNumber)
{
    synth.noteOff(1, midiNoteNumber, 0.0f, true);
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::noteOff(1, midiNoteNumber), 0);
}

void PhotoSynthAudioProcessor::allNotesOff()
{
    synth.allNotesOff(1, true);
    const juce::ScopedLock sl(midiLock);
    internalMidiBuffer.addEvent(juce::MidiMessage::allNotesOff(1), 0);
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Sync BPM from DAW playhead transport if available
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            if (auto hostBpm = pos->getBpm()) {
                currentBpm = juce::jlimit(20.0f, 300.0f, (float)*hostBpm);
            }
        }
    }

    // Merge internal UI MIDI events
    {
        const juce::ScopedLock sl(midiLock);
        if (!internalMidiBuffer.isEmpty()) {
            midiMessages.addEvents(internalMidiBuffer, 0, -1, 0);
            internalMidiBuffer.clear();
        }
    }

    updateMorphState();

    VoiceParams p;
    p.cutoff    = apvts.getRawParameterValue("cutoff")->load();
    p.resonance = apvts.getRawParameterValue("resonance")->load();
    p.lfoRate   = apvts.getRawParameterValue("lfoRate")->load();
    p.lfoDepth  = apvts.getRawParameterValue("lfoDepth")->load();
    p.attack    = apvts.getRawParameterValue("attack")->load();
    p.decay     = apvts.getRawParameterValue("decay")->load();
    p.sustain   = apvts.getRawParameterValue("sustain")->load();
    p.release   = apvts.getRawParameterValue("release")->load();
    p.drive     = apvts.getRawParameterValue("drive")->load();

    if (auto* cp = apvts.getRawParameterValue("color"))
        p.color = cp->load();
    else
        p.color = currentAnalysis.timbreDna * 360.0f;

    if (auto* bp = apvts.getRawParameterValue("brightness"))
        p.brightness = bp->load();
    else
        p.brightness = currentAnalysis.brightness * 100.0f;

    if (auto* sp = apvts.getRawParameterValue("saturation"))
        p.saturation = sp->load();
    else
        p.saturation = currentAnalysis.saturation * 100.0f;

    if (auto* cxp = apvts.getRawParameterValue("complexity"))
        p.complexity = cxp->load();
    else
        p.complexity = currentAnalysis.complexity * 100.0f;

    float archVal = apvts.getRawParameterValue("archetype")->load();
    int archInt = (int)std::round(archVal);
    if (archInt == 1) p.archetype = SynthArchetype::DigitalFmBells;
    else if (archInt == 2) p.archetype = SynthArchetype::HybridWavetable;
    else if (archInt == 0) p.archetype = SynthArchetype::AnalogBrass;
    else p.archetype = currentAnalysis.archetype;
    p.era       = currentAnalysis.temporalEra.era;

    for (int i = 0; i < synth.getNumVoices(); ++i) {
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i))) {
            voice->updatePatch(p);
        }
    }

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Update and process Audio Effects Chain
    photosynth::EffectsParameters fxParams;
    fxParams.delayEnabled       = apvts.getRawParameterValue("delayEnabled")->load() > 0.5f;
    fxParams.delayTimeMs        = apvts.getRawParameterValue("delayTime")->load();
    fxParams.delayFeedback      = apvts.getRawParameterValue("delayFeedback")->load();
    fxParams.delayMix           = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("delayMix")->load() * (0.6f + currentAnalysis.delayMix * 0.8f));

    fxParams.reverbEnabled      = apvts.getRawParameterValue("reverbEnabled")->load() > 0.5f;
    fxParams.reverbRoomSize     = apvts.getRawParameterValue("reverbSize")->load();
    fxParams.reverbDecaySec     = apvts.getRawParameterValue("reverbDecay")->load();
    fxParams.reverbMix          = juce::jlimit(0.0f, 1.0f, apvts.getRawParameterValue("reverbMix")->load() * (0.6f + currentAnalysis.reverbMix * 0.8f));

    fxParams.chorusEnabled      = apvts.getRawParameterValue("chorusEnabled")->load() > 0.5f;
    fxParams.chorusRateHz       = apvts.getRawParameterValue("chorusRate")->load();
    fxParams.chorusDepth        = apvts.getRawParameterValue("chorusDepth")->load();
    fxParams.chorusMix          = apvts.getRawParameterValue("chorusMix")->load();

    fxParams.phaserEnabled      = apvts.getRawParameterValue("phaserEnabled")->load() > 0.5f;
    fxParams.phaserRateHz       = apvts.getRawParameterValue("phaserRate")->load();
    fxParams.phaserDepth        = apvts.getRawParameterValue("phaserDepth")->load();
    fxParams.phaserFeedback     = apvts.getRawParameterValue("phaserFeedback")->load();
    fxParams.phaserMix          = apvts.getRawParameterValue("phaserMix")->load();

    fxParams.flangerEnabled     = apvts.getRawParameterValue("flangerEnabled")->load() > 0.5f;
    fxParams.flangerRateHz      = apvts.getRawParameterValue("flangerRate")->load();
    fxParams.flangerDepth       = apvts.getRawParameterValue("flangerDepth")->load();
    fxParams.flangerMix         = apvts.getRawParameterValue("flangerMix")->load();

    fxParams.distortionEnabled  = apvts.getRawParameterValue("distortionEnabled")->load() > 0.5f;
    fxParams.distortionDrive    = apvts.getRawParameterValue("distortionDrive")->load();
    fxParams.distortionMix      = apvts.getRawParameterValue("distortionMix")->load();

    effectsChain.updateParameters(fxParams);
    effectsChain.processBlock(buffer);

    // Apply master volume gain
    float volDb = apvts.getRawParameterValue("volume")->load();
    float masterGain = juce::Decibels::decibelsToGain(volDb, -60.0f) * 0.95f;
    buffer.applyGain(masterGain);

    // Apply Limiter / Soft-clipping (1:1 with WebAudio dynamics compressor & limiter)
    if (apvts.getRawParameterValue("limiter")->load() > 0.5f) {
        float thresholdDb = apvts.getRawParameterValue("threshold")->load();
        float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* channelData = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float sample = channelData[i];
                if (!std::isfinite(sample)) {
                    sample = 0.0f;
                } else if (std::abs(sample) > thresholdLinear) {
                    float sign = sample > 0.0f ? 1.0f : -1.0f;
                    float excess = std::abs(sample) - thresholdLinear;
                    sample = sign * (thresholdLinear + std::tanh(excess * 0.7f) * (1.0f - thresholdLinear));
                }
                channelData[i] = juce::jlimit(-1.0f, 1.0f, sample);
            }
        }
    } else {
        // Basic safety clamping
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* channelData = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                if (!std::isfinite(channelData[i])) channelData[i] = 0.0f;
                else channelData[i] = juce::jlimit(-1.0f, 1.0f, channelData[i]);
            }
        }
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

static juce::File getPresetsXmlFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("ModularSoundTechnologies")
                    .getChildFile("PhotoSynth");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile("user_presets.xml");
}

void PhotoSynthAudioProcessor::saveCurrentPreset(const juce::String& name, const juce::String& desc)
{
    if (name.isEmpty()) return;

    UserPreset preset;
    preset.name = name;
    preset.description = desc;
    preset.imageA = imageSlotA;
    preset.imageB = imageSlotB;
    preset.slotAName = slotAName;
    preset.slotBName = slotBName;
    preset.xmlState = apvts.copyState().createXml();

    int existingIdx = -1;
    for (size_t i = 0; i < userPresets.size(); ++i) {
        if (userPresets[i].name == name) {
            existingIdx = (int)i;
            break;
        }
    }

    if (existingIdx >= 0) {
        userPresets[(size_t)existingIdx] = std::move(preset);
    } else {
        userPresets.push_back(std::move(preset));
        if (!userPresetNames.contains(name))
            userPresetNames.add(name);
    }

    saveUserPresetsToDisk();
}

juce::StringArray PhotoSynthAudioProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : userPresets)
        names.add(p.name);
    return names;
}

void PhotoSynthAudioProcessor::loadUserPresetByIndex(int index)
{
    if (index < 0 || index >= (int)userPresets.size()) return;
    const auto& preset = userPresets[(size_t)index];

    if (preset.imageA.isValid()) {
        imageSlotA = preset.imageA;
        slotAName = preset.slotAName;
        analysisSlotA = ImageAnalyzer::analyzeImage(imageSlotA);
    }
    if (preset.imageB.isValid()) {
        imageSlotB = preset.imageB;
        slotBName = preset.slotBName;
        analysisSlotB = ImageAnalyzer::analyzeImage(imageSlotB);
    }

    if (preset.xmlState != nullptr) {
        apvts.replaceState(juce::ValueTree::fromXml(*preset.xmlState));
    }

    updateMorphState();
    updateBlendedImage();
}

void PhotoSynthAudioProcessor::loadUserPreset(const juce::String& name)
{
    for (size_t i = 0; i < userPresets.size(); ++i) {
        if (userPresets[i].name == name) {
            loadUserPresetByIndex((int)i);
            break;
        }
    }
}

void PhotoSynthAudioProcessor::saveUserPresetsToDisk()
{
    auto file = getPresetsXmlFile();
    juce::XmlElement root("PhotoSynthUserPresets");

    for (const auto& p : userPresets) {
        auto* pXml = root.createNewChildElement("Preset");
        pXml->setAttribute("name", p.name);
        pXml->setAttribute("description", p.description);
        pXml->setAttribute("slotAName", p.slotAName);
        pXml->setAttribute("slotBName", p.slotBName);

        if (p.xmlState != nullptr) {
            auto* stateElem = pXml->createNewChildElement("State");
            stateElem->addChildElement(new juce::XmlElement(*p.xmlState));
        }
    }

    root.writeTo(file);
}

void PhotoSynthAudioProcessor::loadUserPresetsFromDisk()
{
    auto file = getPresetsXmlFile();
    if (!file.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("PhotoSynthUserPresets")) return;

    userPresets.clear();
    userPresetNames.clear();

    for (auto* pXml : xml->getChildIterator()) {
        if (pXml->hasTagName("Preset")) {
            UserPreset p;
            p.name = pXml->getStringAttribute("name");
            p.description = pXml->getStringAttribute("description");
            p.slotAName = pXml->getStringAttribute("slotAName");
            p.slotBName = pXml->getStringAttribute("slotBName");

            if (auto* stateElem = pXml->getChildByName("State")) {
                if (auto* firstChild = stateElem->getFirstChildElement()) {
                    p.xmlState = std::make_unique<juce::XmlElement>(*firstChild);
                }
            }

            userPresetNames.add(p.name);
            userPresets.push_back(std::move(p));
        }
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
    if (xml != nullptr) {
        xml->setAttribute("slotAName", slotAName);
        xml->setAttribute("slotBName", slotBName);
        xml->setAttribute("currentBpm", (double)currentBpm);

        if (imageSlotA.isValid()) {
            juce::Image thumbA = imageSlotA;
            if (thumbA.getWidth() > 512 || thumbA.getHeight() > 512) {
                int tw = thumbA.getWidth(), th = thumbA.getHeight();
                if (tw > th) { th = std::max(1, (th * 512) / tw); tw = 512; }
                else { tw = std::max(1, (tw * 512) / th); th = 512; }
                thumbA = thumbA.rescaled(tw, th, juce::Graphics::mediumResamplingQuality);
            }
            juce::MemoryOutputStream mos;
            juce::PNGImageFormat png;
            if (png.writeImageToStream(thumbA, mos) && mos.getDataSize() > 0) {
                xml->setAttribute("imageA_b64", juce::Base64::toBase64(mos.getData(), mos.getDataSize()));
            }
        }

        if (imageSlotB.isValid()) {
            juce::Image thumbB = imageSlotB;
            if (thumbB.getWidth() > 512 || thumbB.getHeight() > 512) {
                int tw = thumbB.getWidth(), th = thumbB.getHeight();
                if (tw > th) { th = std::max(1, (th * 512) / tw); tw = 512; }
                else { tw = std::max(1, (tw * 512) / th); th = 512; }
                thumbB = thumbB.rescaled(tw, th, juce::Graphics::mediumResamplingQuality);
            }
            juce::MemoryOutputStream mos;
            juce::PNGImageFormat png;
            if (png.writeImageToStream(thumbB, mos) && mos.getDataSize() > 0) {
                xml->setAttribute("imageB_b64", juce::Base64::toBase64(mos.getData(), mos.getDataSize()));
            }
        }

        copyXmlToBinary (*xml, destData);
    }
}

void PhotoSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr) {
        if (xmlState->hasTagName (apvts.state.getType())) {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        } else if (auto* stateChild = xmlState->getChildByName (apvts.state.getType().toString())) {
            apvts.replaceState (juce::ValueTree::fromXml (*stateChild));
        } else {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }

        if (xmlState->hasAttribute("slotAName"))
            slotAName = xmlState->getStringAttribute("slotAName");
        if (xmlState->hasAttribute("slotBName"))
            slotBName = xmlState->getStringAttribute("slotBName");
        if (xmlState->hasAttribute("currentBpm"))
            currentBpm = (float)xmlState->getDoubleAttribute("currentBpm", 120.0);

        if (xmlState->hasAttribute("imageA_b64")) {
            juce::MemoryOutputStream mos;
            juce::Base64::convertFromBase64(mos, xmlState->getStringAttribute("imageA_b64"));
            auto img = juce::ImageFileFormat::loadFrom(mos.getData(), mos.getDataSize());
            if (img.isValid()) {
                imageSlotA = img;
                analysisSlotA = ImageAnalyzer::analyzeImage(img);
            }
        }

        if (xmlState->hasAttribute("imageB_b64")) {
            juce::MemoryOutputStream mos;
            juce::Base64::convertFromBase64(mos, xmlState->getStringAttribute("imageB_b64"));
            auto img = juce::ImageFileFormat::loadFrom(mos.getData(), mos.getDataSize());
            if (img.isValid()) {
                imageSlotB = img;
                analysisSlotB = ImageAnalyzer::analyzeImage(img);
            }
        }

        updateMorphState();
        updateBlendedImage();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
