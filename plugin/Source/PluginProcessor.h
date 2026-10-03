#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

//==============================================================================
/** Polyphonic Synth Voice using PolyBLEP anti-aliased oscillators */
struct Voice
{
    int noteNumber = -1;
    float velocity = 0.0f;
    float currentPitchHz = 440.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    
    // 2-pole state variable filter states
    float s1 = 0.0f;
    float s2 = 0.0f;

    // ADSR envelope
    juce::ADSR ampAdsr;

    void start (int note, float vel, double sampleRate)
    {
        noteNumber = note;
        velocity = juce::jlimit (0.05f, 1.0f, vel);
        currentPitchHz = (float) juce::MidiMessage::getMidiNoteInHertz (noteNumber);
        phase = 0.0f;
        phaseIncrement = currentPitchHz / (float) (sampleRate > 1000.0 ? sampleRate : 44100.0);
        s1 = 0.0f;
        s2 = 0.0f;

        juce::ADSR::Parameters params;
        params.attack  = 0.015f;
        params.decay   = 0.35f;
        params.sustain = 0.75f;
        params.release = 0.30f;
        ampAdsr.setSampleRate (sampleRate);
        ampAdsr.setParameters (params);
        ampAdsr.noteOn();
    }

    void stop (bool allowTailOff)
    {
        if (allowTailOff)
            ampAdsr.noteOff();
        else
        {
            ampAdsr.reset();
            noteNumber = -1;
        }
    }

    bool isActive() const
    {
        return noteNumber != -1 && ampAdsr.isActive();
    }
};

//==============================================================================
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

    const juce::String getName() const override { return "Paint a Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    #endif

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // State Serialization
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS
    juce::AudioProcessorValueTreeState apvts;

    // Live Scope FIFO for Native Editor Visualizer
    static constexpr int scopeSize = 256;
    void getScopeData (float* dest, int numSamples);

private:
    static constexpr int maxVoices = 16;
    std::array<Voice, maxVoices> voices;
    double currentSampleRate = 44100.0;

    // Scope buffer
    std::array<float, scopeSize> scopeBuffer {};
    int scopeWriteIndex = 0;
    juce::CriticalSection scopeLock;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    Voice* findFreeVoice();
    Voice* findVoiceForNote (int noteNumber);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioProcessor)
};
