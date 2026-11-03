/*
  ==============================================================================
    PluginProcessor.h
    Modular Sound Technologies - Paint a Synth (Photo Synth)
    JUCE 8 Hybrid WebView Plugin Architecture
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ImageAnalyzer.h"
#include "SynthEngine.h"
#include "EffectsChain.h"
#include "PresetFactory.h"
#include <array>
#include <atomic>

class PhotoSynthAudioProcessor : public juce::AudioProcessor
{
public:
    PhotoSynthAudioProcessor();
    ~PhotoSynthAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Photo Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool isSynth() const { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Pure Software Instrument (no input audio bus)
    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // APVTS Explicit Binary State Serialization (fixes reset bug)
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Image Slot Loading & Morphing
    void loadSlotAImage (const juce::Image& img, const juce::String& name = {});
    void loadSlotBImage (const juce::Image& img, const juce::String& name = {});
    void loadSlotAPreset (int presetIndex);
    void loadSlotBPreset (int presetIndex);

    juce::Image getImageA() const { return imageSlotA; }
    juce::Image getImageB() const { return imageSlotB; }
    juce::Image getBlendedImage();

    juce::String getSlotAName() const { return slotAName; }
    juce::String getSlotBName() const { return slotBName; }
    juce::String getImageSlotAAsDataUrl() const;
    juce::String getImageSlotBAsDataUrl() const;
    /** Decodes a frontend data URL (PNG/JPEG/GIF), analyses it and makes it the slot image.
        Returns false if the image could not be decoded. Message thread only. */
    bool syncSlotImageFromFrontend (const juce::String& slot, const juce::String& dataUrl, const juce::String& name);

    /** Body of the WebView "syncPatch" native function. Writes every known key of the
        frontend patch object (already in APVTS units, e.g. cutoff Hz, brightness 0-100,
        archetype 0/1/2) into the APVTS. processBlock pushes the APVTS into the voices at
        the start of every block. Unknown keys are ignored. Returns number of params set. */
    int applyFrontendPatch (const juce::var& patchObject);

    /** Body of the WebView "getInitialState" native function. Parameter values are in
        real units (not 0-1 normalised). */
    juce::var getInitialStateForFrontend() const;

    bool hasFrontendState() const { return frontendStateKnown; }

    /** Message thread: analysis for the current morph position (A/B interpolation). */
    ImageAnalysisResult getLatestAnalysis() const;
    ImageAnalysisResult getAnalysisA() const { return analysisSlotA; }
    ImageAnalysisResult getAnalysisB() const { return analysisSlotB; }

    juce::AudioProcessorValueTreeState apvts;

    // Real-time oscilloscope FIFO buffer for WebView
    static constexpr int scopeBufferSize = 512;
    void getScopeData (float* destination, int numSamples);

    // Virtual Keyboard Trigger Helpers for UI
    void noteOn (int midiNoteNumber, float velocity, int midiChannel = 1);
    void noteOff (int midiNoteNumber, int midiChannel = 1);
    void allNotesOff (int midiChannel = 0);

    void updateParametersFromAnalysis();
    void applyAnalysisToParameters (const ImageAnalysisResult& analysis);

    float getBpm() const { return currentBpm.load(); }
    void setBpm (float newBpm);

private:
    //==========================================================================
    // Custom synth engine (owns juce::Synthesiser + 16 SynthVoices + shared patch).
    // Fully initialised in the constructor so DAW MIDI never hits an empty/
    // unprepared voice pool.
    PhotoSynthEngine engine;

    //==========================================================================
    // UI virtual keyboard -> audio thread MIDI FIFO (lock-free on the audio side).
    // Producers (WebView native-function callbacks) serialise on a SpinLock that the
    // audio thread never touches; the audio thread only reads the AbstractFifo.
    struct UiMidiEvent { juce::uint8 data[3] { 0, 0, 0 }; int size { 0 }; };
    static constexpr int uiMidiFifoCapacity = 1024;
    juce::AbstractFifo uiMidiFifo { uiMidiFifoCapacity };
    std::array<UiMidiEvent, uiMidiFifoCapacity> uiMidiEvents {};
    juce::SpinLock uiMidiWriteLock;
    void pushUiMidi (const juce::MidiMessage& message);
    void drainUiMidiInto (juce::MidiBuffer& midiMessages);

    //==========================================================================
    // Cached APVTS parameter pointers (avoids String construction + map lookups
    // on the audio thread every block).
    struct ParamPointers
    {
        std::atomic<float>* cutoff {}; std::atomic<float>* resonance {};
        std::atomic<float>* attack {}; std::atomic<float>* decay {};
        std::atomic<float>* sustain {}; std::atomic<float>* release {};
        std::atomic<float>* drive {}; std::atomic<float>* volume {};
        std::atomic<float>* morph {}; std::atomic<float>* archetype {};
        std::atomic<float>* color {}; std::atomic<float>* brightness {};
        std::atomic<float>* saturation {}; std::atomic<float>* complexity {};
        std::atomic<float>* lfoRate {}; std::atomic<float>* lfoDepth {};
        std::atomic<float>* delayEnabled {}; std::atomic<float>* delayTime {};
        std::atomic<float>* delayFeedback {}; std::atomic<float>* delayMix {};
        std::atomic<float>* reverbEnabled {}; std::atomic<float>* reverbSize {};
        std::atomic<float>* reverbDecay {}; std::atomic<float>* reverbMix {};
        std::atomic<float>* chorusEnabled {}; std::atomic<float>* chorusRate {};
        std::atomic<float>* chorusDepth {}; std::atomic<float>* chorusMix {};
    } paramPtrs;
    void cacheParameterPointers();

    //==========================================================================
    // Realtime-safe (POD) subset of the image analysis. ImageAnalysisResult holds
    // juce::Strings, so it must never be copied/interpolated on the audio thread.
    struct AnalysisSnapshot
    {
        SynthArchetype archetype { SynthArchetype::HybridWavetable };
        TemporalEra era { TemporalEra::Era1970s };
        float delayMix { 0.2f };
        float reverbMix { 0.3f };

        static AnalysisSnapshot from (const ImageAnalysisResult& r) noexcept;
        static AnalysisSnapshot interpolate (const AnalysisSnapshot& a, const AnalysisSnapshot& b, float t) noexcept;
    };
    AnalysisSnapshot sharedSnapA, sharedSnapB;   // written by message thread under analysisLock
    AnalysisSnapshot rtSnapA, rtSnapB;           // audio-thread private copies
    juce::SpinLock analysisLock;
    void publishAnalysisSnapshots();

    //==========================================================================
    juce::Image imageSlotA;
    juce::Image imageSlotB;
    juce::Image blendedImage;
    juce::String slotAName { "Synthwave Sunset" };
    juce::String slotBName { "Obsidian Crypt" };

    // Exact data URLs the frontend sent (the photos the user picked). Persisted in the
    // plugin state so a reopened project / reopened editor shows and plays the same photo.
    juce::String slotAFrontendDataUrl, slotBFrontendDataUrl;
    // True once the host restored a state or the frontend pushed a patch, i.e. the APVTS
    // holds a deliberate patch the UI must adopt rather than overwrite with its defaults.
    bool frontendStateKnown { false };

    ImageAnalysisResult analysisSlotA;
    ImageAnalysisResult analysisSlotB;
    ImageAnalysisResult currentAnalysis;   // message thread only

    photosynth::EffectsChain effectsChain;
    juce::dsp::Limiter<float> masterLimiter;

    float scopeBuffer[scopeBufferSize] { 0.0f };
    int scopeBufferWriteIndex { 0 };
    juce::SpinLock scopeLock;

    /** Audio thread (or before audio starts): APVTS + analysis -> engine patch. */
    void pushParametersToEngine();
    VoiceParams buildVoiceParams() const noexcept;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static int presetIndexForName (const juce::String& name);
    void updateMorphState();
    void updateBlendedImage();

    std::atomic<float> currentBpm { 120.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioProcessor)
};
