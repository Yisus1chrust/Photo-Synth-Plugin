/*
  ==============================================================================
    SynthEngine.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "ImageAnalyzer.h"

struct VoiceParams
{
    float cutoff { 2000.0f };
    float resonance { 1.0f };
    float attack { 0.01f };
    float decay { 0.5f };
    float sustain { 0.7f };
    float release { 0.3f };
    float lfoRate { 2.0f };
    float lfoDepth { 0.2f };
    float drive { 0.0f };
    float color { 88.0f };         // 0 to 360 degrees
    float brightness { 45.0f };    // 0 to 100
    float saturation { 83.0f };    // 0 to 100
    float complexity { 52.0f };    // 0 to 100
    SynthArchetype archetype { SynthArchetype::HybridWavetable };
    TemporalEra era { TemporalEra::Era1970s };
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    SynthVoice();
    ~SynthVoice() override = default;

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepare(double sampleRate, int samplesPerBlock);
    void updatePatch(const VoiceParams& params);
    void setSharedParams(const VoiceParams* sharedParams) { masterParams = sharedParams; }

    /** Clamps any out-of-range / uninitialised field to a playable default. */
    static void sanitise(VoiceParams& params);

    static constexpr float pitchBendRangeSemitones = 2.0f;

private:
    void applyEnvelopeParams();
    static float pitchWheelToRatio(int pitchWheelValue);

    const VoiceParams* masterParams { nullptr };
    double currentSampleRate { 44100.0 };
    float currentPitchHz { 440.0f };
    float pitchBendRatio { 1.0f };
    float phaseIncrement { 0.0f };
    float currentVelocity { 0.8f };
    VoiceParams currentParams;

    // Per-sample oscillator phase accumulators
    float phase1 { 0.0f };
    float phase2 { 0.0f };
    float phaseSub { 0.0f };
    float phaseMod { 0.0f };
    float lfoPhase { 0.0f };

    // 2-pole resonant stereo filter state
    float filterS1[2] { 0.0f, 0.0f };
    float filterS2[2] { 0.0f, 0.0f };

    juce::ADSR ampEnv;
    juce::ADSR filterEnv;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};

class PhotoSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

/**
    PhotoSynthEngine
    Owns the juce::Synthesiser, the 16 custom SynthVoice instances, the shared
    master patch and the PhotoSynthSound. All voice allocation and MIDI dispatch
    happens on the audio thread through render().

    The engine is fully playable straight after construction (default 44.1 kHz
    and default patch) so a host that sends MIDI immediately after instantiating
    the plugin - before the WebView UI has loaded or called syncPatch - always
    reaches the custom oscillator/filter engine.
*/
class PhotoSynthEngine
{
public:
    static constexpr int numVoices = 16;

    PhotoSynthEngine();

    /** Message thread / host prepare: sets sample rate on synth + voices and resets them. */
    void prepare(double sampleRate, int samplesPerBlock);

    /** Hard-stops every voice (no tail). Call only when audio is not running or from the audio thread. */
    void reset();

    /** Audio thread: pushes a new master patch into the shared params and every voice. */
    void setPatch(const VoiceParams& params);

    /** Audio thread: dispatches every MIDI event (note on/off, pitch wheel, sustain,
        all-notes-off, ...) sample-accurately to the custom voices and renders them. */
    void render(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples);

    void allNotesOff(int midiChannel, bool allowTailOff) { synth.allNotesOff(midiChannel, allowTailOff); }

    /** True when every voice is a SynthVoice and can play the registered PhotoSynthSound. */
    bool isReady() const noexcept { return ready; }
    int getNumVoices() const { return synth.getNumVoices(); }
    const VoiceParams& getPatch() const noexcept { return sharedParams; }

private:
    void buildVoices();
    bool validateCustomVoices() const;

    juce::Synthesiser synth;
    VoiceParams sharedParams;
    bool ready { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhotoSynthEngine)
};
