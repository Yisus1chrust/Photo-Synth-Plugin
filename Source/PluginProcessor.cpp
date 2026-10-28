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
    // The custom engine (sound + 16 SynthVoices + shared patch) is fully built by
    // PhotoSynthEngine's constructor - no UI callback is needed to make it playable.
    cacheParameterPointers();

    // Default starting presets matching the web UI
    imageSlotA = photosynth::PresetFactory::createSynthwaveSunset();
    imageSlotB = photosynth::PresetFactory::createObsidianCrypt();
    analysisSlotA = ImageAnalyzer::analyzeImage (imageSlotA);
    analysisSlotB = ImageAnalyzer::analyzeImage (imageSlotB);
    slotAName = "Synthwave Sunset";
    slotBName = "Obsidian Crypt";

    publishAnalysisSnapshots();
    updateMorphState();
    updateBlendedImage();

    // Copy the default preset's analysis (incl. the synth archetype) into the APVTS
    // immediately. Previously the archetype was only set by the React UI's syncPatch,
    // so a DAW playing MIDI before/without the editor got archetype 0 (plain
    // AnalogBrass saw stack) instead of the image-derived engine.
    applyAnalysisToParameters (currentAnalysis);

    // Push the resulting patch into every voice now (not on the first UI message).
    pushParametersToEngine();
    jassert (engine.isReady());
}

void PhotoSynthAudioProcessor::cacheParameterPointers()
{
    auto get = [this] (const char* id)
    {
        auto* ptr = apvts.getRawParameterValue (id);
        jassert (ptr != nullptr); // parameter ID missing from createParameterLayout()
        return ptr;
    };

    paramPtrs.cutoff        = get ("cutoff");
    paramPtrs.resonance     = get ("resonance");
    paramPtrs.attack        = get ("attack");
    paramPtrs.decay         = get ("decay");
    paramPtrs.sustain       = get ("sustain");
    paramPtrs.release       = get ("release");
    paramPtrs.drive         = get ("drive");
    paramPtrs.volume        = get ("volume");
    paramPtrs.morph         = get ("morph");
    paramPtrs.archetype     = get ("archetype");
    paramPtrs.color         = get ("color");
    paramPtrs.brightness    = get ("brightness");
    paramPtrs.saturation    = get ("saturation");
    paramPtrs.complexity    = get ("complexity");
    paramPtrs.lfoRate       = get ("lfoRate");
    paramPtrs.lfoDepth      = get ("lfoDepth");
    paramPtrs.delayEnabled  = get ("delayEnabled");
    paramPtrs.delayTime     = get ("delayTime");
    paramPtrs.delayFeedback = get ("delayFeedback");
    paramPtrs.delayMix      = get ("delayMix");
    paramPtrs.reverbEnabled = get ("reverbEnabled");
    paramPtrs.reverbSize    = get ("reverbSize");
    paramPtrs.reverbDecay   = get ("reverbDecay");
    paramPtrs.reverbMix     = get ("reverbMix");
    paramPtrs.chorusEnabled = get ("chorusEnabled");
    paramPtrs.chorusRate    = get ("chorusRate");
    paramPtrs.chorusDepth   = get ("chorusDepth");
    paramPtrs.chorusMix     = get ("chorusMix");
}

PhotoSynthAudioProcessor::AnalysisSnapshot PhotoSynthAudioProcessor::AnalysisSnapshot::from (const ImageAnalysisResult& r) noexcept
{
    AnalysisSnapshot s;
    s.archetype = r.archetype;
    s.era       = r.temporalEra.era;
    s.delayMix  = r.delayMix;
    s.reverbMix = r.reverbMix;
    return s;
}

PhotoSynthAudioProcessor::AnalysisSnapshot PhotoSynthAudioProcessor::AnalysisSnapshot::interpolate (const AnalysisSnapshot& a, const AnalysisSnapshot& b, float t) noexcept
{
    AnalysisSnapshot s;
    s.archetype = t < 0.5f ? a.archetype : b.archetype;
    s.era       = t < 0.5f ? a.era : b.era;
    s.delayMix  = a.delayMix  + (b.delayMix  - a.delayMix)  * t;
    s.reverbMix = a.reverbMix + (b.reverbMix - a.reverbMix) * t;
    return s;
}

void PhotoSynthAudioProcessor::publishAnalysisSnapshots()
{
    const auto a = AnalysisSnapshot::from (analysisSlotA);
    const auto b = AnalysisSnapshot::from (analysisSlotB);
    const juce::SpinLock::ScopedLockType sl (analysisLock);
    sharedSnapA = a;
    sharedSnapB = b;
}

int PhotoSynthAudioProcessor::presetIndexForName (const juce::String& name)
{
    static const char* const names[] = { "Synthwave Sunset", "Obsidian Crypt", "Bioluminescent Coral", "Desert Dunes", "Cyberpunk Alley" };
    for (int i = 0; i < (int) std::size (names); ++i)
        if (name == names[i])
            return i;
    return -1;
}

PhotoSynthAudioProcessor::~PhotoSynthAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PhotoSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Core Synthesis Parameters
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "cutoff", 1 }, "Cutoff Frequency", juce::NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f), 2400.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "resonance", 1 }, "Resonance", juce::NormalisableRange<float> (0.1f, 10.0f, 0.05f), 1.2f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack Time", juce::NormalisableRange<float> (0.001f, 5.0f, 0.001f, 0.35f), 0.030f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "Decay Time", juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.35f), 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sustain", 1 }, "Sustain Level", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release Time", juce::NormalisableRange<float> (0.01f, 8.0f, 0.01f, 0.35f), 0.45f));

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
        juce::ParameterID { "lfoDepth", 1 }, "LFO Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

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

VoiceParams PhotoSynthAudioProcessor::buildVoiceParams() const noexcept
{
    // Realtime-safe: atomics + POD snapshots only (no Strings, no locks, no allocation).
    const float morphT = paramPtrs.morph->load();
    const auto analysis = AnalysisSnapshot::interpolate (rtSnapA, rtSnapB, morphT);

    VoiceParams v;
    v.cutoff     = paramPtrs.cutoff->load();
    v.resonance  = paramPtrs.resonance->load();
    v.lfoRate    = paramPtrs.lfoRate->load();
    v.lfoDepth   = paramPtrs.lfoDepth->load();
    v.attack     = paramPtrs.attack->load();
    v.decay      = paramPtrs.decay->load();
    v.sustain    = paramPtrs.sustain->load();
    v.release    = paramPtrs.release->load();
    v.drive      = paramPtrs.drive->load();
    v.color      = paramPtrs.color->load();
    v.brightness = paramPtrs.brightness->load();
    v.saturation = paramPtrs.saturation->load();
    v.complexity = paramPtrs.complexity->load();

    switch ((int) std::round (paramPtrs.archetype->load()))
    {
        case 0:  v.archetype = SynthArchetype::AnalogBrass;     break;
        case 1:  v.archetype = SynthArchetype::DigitalFmBells;  break;
        case 2:  v.archetype = SynthArchetype::HybridWavetable; break;
        default: v.archetype = analysis.archetype;              break;
    }
    v.era = analysis.era;
    return v;
}

void PhotoSynthAudioProcessor::pushParametersToEngine()
{
    // Pick up the latest image analysis published by the message thread. Never block:
    // if the UI is mid-update we simply keep last block's snapshot.
    {
        const juce::SpinLock::ScopedTryLockType tl (analysisLock);
        if (tl.isLocked())
        {
            rtSnapA = sharedSnapA;
            rtSnapB = sharedSnapB;
        }
    }

    engine.setPatch (buildVoiceParams());
}

void PhotoSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 1. Prepare/reset the custom voice engine (validates the custom voice pool,
    //    sets sample rate on synth + every voice, hard-stops stale notes).
    engine.prepare (sampleRate, samplesPerBlock);

    // 2. Explicitly push the current (default or host-restored) patch into all voices
    //    BEFORE the host starts calling processBlock with MIDI.
    pushParametersToEngine();

    // 3. Drop any UI notes queued while the plugin was inactive (avoids stuck notes).
    {
        const juce::SpinLock::ScopedLockType sl (uiMidiWriteLock);
        uiMidiFifo.reset();
    }

    const auto numOutputs = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), numOutputs };
    effectsChain.prepare (spec);
    masterLimiter.prepare (spec);
}

void PhotoSynthAudioProcessor::releaseResources()
{
    engine.reset();
    effectsChain.reset();
}

void PhotoSynthAudioProcessor::pushUiMidi (const juce::MidiMessage& message)
{
    // Called from the message thread (WebView native functions). Never from audio.
    const juce::SpinLock::ScopedLockType sl (uiMidiWriteLock);

    int start1, size1, start2, size2;
    uiMidiFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 < 1)
    {
        DBG ("PhotoSynth: UI MIDI FIFO full - dropping event");
        return;
    }

    auto& e = uiMidiEvents[(size_t) (size1 > 0 ? start1 : start2)];
    e.size = juce::jmin (3, message.getRawDataSize());
    std::memcpy (e.data, message.getRawData(), (size_t) e.size);
    uiMidiFifo.finishedWrite (1);
}

void PhotoSynthAudioProcessor::drainUiMidiInto (juce::MidiBuffer& midiMessages)
{
    // Audio thread: lock-free single-consumer read.
    const int numReady = uiMidiFifo.getNumReady();
    if (numReady <= 0)
        return;

    int start1, size1, start2, size2;
    uiMidiFifo.prepareToRead (numReady, start1, size1, start2, size2);

    auto add = [&] (int start, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            const auto& e = uiMidiEvents[(size_t) (start + i)];
            if (e.size > 0)
                midiMessages.addEvent (e.data, e.size, 0);
        }
    };
    add (start1, size1);
    add (start2, size2);
    uiMidiFifo.finishedRead (size1 + size2);
}

void PhotoSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // 1. Instrument: start from silence (no input bus; never pass host residue through)
    buffer.clear();

    // 2. Sync DAW BPM Transport
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            if (auto hostBpm = pos->getBpm()) {
                currentBpm.store (juce::jlimit (20.0f, 300.0f, (float) *hostBpm));
            }
        }
    }

    // 3. Bind live APVTS parameters + image analysis to all voices BEFORE any MIDI is
    //    handled, so the very first DAW note-on already uses the custom patch.
    //    Lock-free: atomics + try-locked POD snapshot only.
    pushParametersToEngine();

    // 4. Merge on-screen keyboard notes (lock-free FIFO) into the host MIDI stream.
    //    Host MIDI is ALWAYS processed - there is no dependency on the UI having
    //    sent anything, and no lock shared with the UI thread is taken here.
    drainUiMidiInto (midiMessages);

    if (numSamples == 0 || buffer.getNumChannels() == 0)
        return;

    // 5. Dispatch ALL MIDI (DAW timeline, DAW virtual keyboard, UI keyboard) to the
    //    custom voices. juce::Synthesiser iterates the MidiBuffer and handles noteOn,
    //    noteOff, pitchWheel, sustain/sostenuto, allNotesOff/allSoundOff sample-accurately;
    //    voices are allocated/started right here on the audio thread.
    engine.render (buffer, midiMessages, 0, numSamples);

    // 6. Effects Chain & Master Gain
    const auto analysis = AnalysisSnapshot::interpolate (rtSnapA, rtSnapB, paramPtrs.morph->load());

    photosynth::EffectsParameters fxParams;
    fxParams.delayEnabled   = paramPtrs.delayEnabled->load() > 0.5f;
    fxParams.delayTimeMs    = paramPtrs.delayTime->load();
    fxParams.delayFeedback  = paramPtrs.delayFeedback->load();
    fxParams.delayMix       = juce::jlimit (0.0f, 1.0f, paramPtrs.delayMix->load() * (0.6f + analysis.delayMix * 0.8f));

    fxParams.reverbEnabled  = paramPtrs.reverbEnabled->load() > 0.5f;
    fxParams.reverbRoomSize = paramPtrs.reverbSize->load();
    fxParams.reverbDecaySec = paramPtrs.reverbDecay->load();
    fxParams.reverbMix      = juce::jlimit (0.0f, 1.0f, paramPtrs.reverbMix->load() * (0.6f + analysis.reverbMix * 0.8f));

    fxParams.chorusEnabled  = paramPtrs.chorusEnabled->load() > 0.5f;
    fxParams.chorusRateHz   = paramPtrs.chorusRate->load();
    fxParams.chorusDepth    = paramPtrs.chorusDepth->load();
    fxParams.chorusMix      = paramPtrs.chorusMix->load();

    effectsChain.updateParameters (fxParams);
    effectsChain.processBlock (buffer);

    const float volDb = paramPtrs.volume->load();
    const float masterGain = juce::Decibels::decibelsToGain (volDb, -60.0f) * 0.35f;
    buffer.applyGain (masterGain);

    // 7. Oscilloscope capture - try-lock so the audio thread never waits on the UI
    {
        const juce::SpinLock::ScopedTryLockType tl (scopeLock);
        if (tl.isLocked()) {
            const float* leftChannel = buffer.getReadPointer (0);
            for (int i = 0; i < numSamples; ++i) {
                scopeBuffer[scopeBufferWriteIndex] = leftChannel[i];
                scopeBufferWriteIndex = (scopeBufferWriteIndex + 1) % scopeBufferSize;
            }
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
        // The user's photos (UI-downscaled JPEG data URLs, typically < 150 KB each).
        if (slotAFrontendDataUrl.isNotEmpty()) xml->setAttribute ("slotADataUrl", slotAFrontendDataUrl);
        if (slotBFrontendDataUrl.isNotEmpty()) xml->setAttribute ("slotBDataUrl", slotBFrontendDataUrl);

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
            currentBpm.store ((float) xmlState->getDoubleAttribute ("currentBpm", 120.0));

        // Restore the slot photos (and therefore their analysis) so a reopened DAW
        // project sounds the same without the UI having to re-sync. Prefer the saved
        // user photos; fall back to the factory preset renders for older projects.
        const auto savedA = xmlState->getStringAttribute ("slotADataUrl");
        const auto savedB = xmlState->getStringAttribute ("slotBDataUrl");
        const auto restoredNameA = slotAName, restoredNameB = slotBName;
        slotAFrontendDataUrl.clear();
        slotBFrontendDataUrl.clear();

        if (! (savedA.isNotEmpty() && syncSlotImageFromFrontend ("A", savedA, restoredNameA)))
            if (const int idxA = presetIndexForName (slotAName); idxA >= 0) loadSlotAPreset (idxA);
        if (! (savedB.isNotEmpty() && syncSlotImageFromFrontend ("B", savedB, restoredNameB)))
            if (const int idxB = presetIndexForName (slotBName); idxB >= 0) loadSlotBPreset (idxB);

        // The restored APVTS is a deliberate patch: the UI must adopt it, not overwrite it.
        frontendStateKnown = true;

        // Message thread: only publish data. The audio thread picks the new patch up
        // at the start of its next processBlock (no direct voice mutation from here).
        publishAnalysisSnapshots();
        updateMorphState();
        updateBlendedImage();
    }
}

void PhotoSynthAudioProcessor::noteOn (int midiNoteNumber, float velocity, int midiChannel)
{
    const int ch = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    pushUiMidi (juce::MidiMessage::noteOn (ch, juce::jlimit (0, 127, midiNoteNumber), juce::jlimit (0.0f, 1.0f, velocity)));
}

void PhotoSynthAudioProcessor::noteOff (int midiNoteNumber, int midiChannel)
{
    const int ch = (midiChannel >= 1 && midiChannel <= 16) ? midiChannel : 1;
    pushUiMidi (juce::MidiMessage::noteOff (ch, juce::jlimit (0, 127, midiNoteNumber)));
}

void PhotoSynthAudioProcessor::allNotesOff (int midiChannel)
{
    // Queued only - the audio thread performs the actual voice release.
    if (midiChannel >= 1 && midiChannel <= 16) {
        pushUiMidi (juce::MidiMessage::allNotesOff (midiChannel));
    } else {
        for (int ch = 1; ch <= 16; ++ch)
            pushUiMidi (juce::MidiMessage::allNotesOff (ch));
    }
}

void PhotoSynthAudioProcessor::getScopeData (float* destination, int numSamples)
{
    numSamples = juce::jlimit (0, scopeBufferSize, numSamples);
    const juce::SpinLock::ScopedLockType sl (scopeLock);
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
        publishAnalysisSnapshots();
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
        publishAnalysisSnapshots();
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
    if (slotAFrontendDataUrl.isNotEmpty()) return slotAFrontendDataUrl;
    if (!imageSlotA.isValid()) return {};
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat pngFormat;
    pngFormat.writeImageToStream (imageSlotA, mos);
    return "data:image/png;base64," + juce::Base64::toBase64 (mos.getData(), mos.getDataSize());
}

juce::String PhotoSynthAudioProcessor::getImageSlotBAsDataUrl() const
{
    if (slotBFrontendDataUrl.isNotEmpty()) return slotBFrontendDataUrl;
    if (!imageSlotB.isValid()) return {};
    juce::MemoryOutputStream mos;
    juce::PNGImageFormat pngFormat;
    pngFormat.writeImageToStream (imageSlotB, mos);
    return "data:image/png;base64," + juce::Base64::toBase64 (mos.getData(), mos.getDataSize());
}

static juce::Image decodeDataUrlImage (const juce::String& dataUrl)
{
    const int commaIdx = dataUrl.indexOf (",");
    if (commaIdx < 0)
        return {};

    juce::MemoryOutputStream mos;
    if (! juce::Base64::convertFromBase64 (mos, dataUrl.substring (commaIdx + 1)))
        return {};

    // Auto-detects PNG / JPEG / GIF (the UI sends canvas JPEGs).
    return juce::ImageFileFormat::loadFrom (mos.getData(), mos.getDataSize());
}

bool PhotoSynthAudioProcessor::syncSlotImageFromFrontend (const juce::String& slot, const juce::String& dataUrl, const juce::String& name)
{
    const auto decodedImg = decodeDataUrlImage (dataUrl);
    if (! decodedImg.isValid())
    {
        DBG ("PhotoSynth: syncSlotImage could not decode image for slot " << slot);
        return false;
    }

    if (slot == "B")
    {
        slotBFrontendDataUrl = dataUrl;
        loadSlotBImage (decodedImg, name);
    }
    else
    {
        slotAFrontendDataUrl = dataUrl;
        loadSlotAImage (decodedImg, name);
    }
    return true;
}

int PhotoSynthAudioProcessor::applyFrontendPatch (const juce::var& patchObject)
{
    auto* obj = patchObject.getDynamicObject();
    if (obj == nullptr)
        return 0;

    int numSet = 0;
    bool morphChanged = false;

    for (const auto& prop : obj->getProperties())
    {
        juce::String paramId = prop.name.toString();

        // Accept the WebAudio patch key names as aliases.
        if (paramId == "cutoffOffset")        paramId = "cutoff";
        else if (paramId == "attackTime")     paramId = "attack";
        else if (paramId == "decayTime")      paramId = "decay";
        else if (paramId == "sustainLevel")   paramId = "sustain";
        else if (paramId == "releaseTime")    paramId = "release";
        else if (paramId == "masterVolume")   paramId = "volume";
        else if (paramId == "limiterEnabled") paramId = "limiter";

        const auto& v = prop.value;
        if (! (v.isDouble() || v.isInt() || v.isInt64() || v.isBool()))
            continue;

        const float value = static_cast<float> ((double) v);
        if (! std::isfinite (value))
            continue;

        // NOTE: no "<= 1 means normalised" auto-scaling here. PluginBridge.syncFullPatch
        // always sends APVTS units, and the old heuristic turned e.g. brightness 0.8%
        // into 80%.
        if (auto* param = apvts.getParameter (paramId))
        {
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            ++numSet;
            morphChanged = morphChanged || paramId == "morph";
        }
    }

    if (morphChanged)
    {
        updateMorphState();
        updateBlendedImage();
    }

    if (numSet > 0)
        frontendStateKnown = true;

    return numSet;
}

juce::var PhotoSynthAudioProcessor::getInitialStateForFrontend() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("bpm", (double) getBpm());
    obj->setProperty ("isNativeHost", true);
    obj->setProperty ("hasState", frontendStateKnown);
    obj->setProperty ("slotAName", slotAName);
    obj->setProperty ("slotBName", slotBName);
    // Only hand back photos that came from the UI (or a saved project). The C++
    // procedural preset renders are NOT the UI's preset photos and must not replace them.
    obj->setProperty ("slotADataUrl", slotAFrontendDataUrl);
    obj->setProperty ("slotBDataUrl", slotBFrontendDataUrl);
    obj->setProperty ("morph", (double) paramPtrs.morph->load());

    auto* paramsObj = new juce::DynamicObject();
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            paramsObj->setProperty (p->paramID, (double) p->convertFrom0to1 (p->getValue()));

    obj->setProperty ("parameters", juce::var (paramsObj));
    return juce::var (obj);
}

void PhotoSynthAudioProcessor::updateMorphState()
{
    // Message thread only (ImageAnalysisResult contains juce::Strings).
    currentAnalysis = getLatestAnalysis();
}

ImageAnalysisResult PhotoSynthAudioProcessor::getLatestAnalysis() const
{
    return ImageAnalyzer::interpolate (analysisSlotA, analysisSlotB, paramPtrs.morph->load());
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

    // Archetype selects the oscillator engine. Without this the APVTS stayed at its
    // layout default (0 = AnalogBrass) until the React UI called syncPatch.
    const float archetypeValue = analysis.archetype == SynthArchetype::DigitalFmBells  ? 1.0f
                               : analysis.archetype == SynthArchetype::HybridWavetable ? 2.0f
                                                                                       : 0.0f;
    if (auto* p = apvts.getParameter ("archetype")) p->setValueNotifyingHost (p->convertTo0to1 (archetypeValue));
}

void PhotoSynthAudioProcessor::updateParametersFromAnalysis()
{
    updateMorphState();
    applyAnalysisToParameters (currentAnalysis);
}

void PhotoSynthAudioProcessor::setBpm (float newBpm)
{
    currentBpm.store (juce::jlimit (20.0f, 300.0f, newBpm));
}

juce::AudioProcessorEditor* PhotoSynthAudioProcessor::createEditor()
{
    return new PhotoSynthAudioEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhotoSynthAudioProcessor();
}
