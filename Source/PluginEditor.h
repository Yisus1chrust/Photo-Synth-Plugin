/*
  ==============================================================================
    PluginEditor.h
    Modular Sound Technologies - Paint a Synth (Photo Synth)
    JUCE 8 Hybrid WebView Plugin Architecture
  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PhotoSynthAudioEditor : public juce::AudioProcessorEditor,
                              private juce::ChangeListener,
                              private juce::Timer
{
public:
    explicit PhotoSynthAudioEditor (PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PhotoSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::WebBrowserComponent> webBrowser;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Editor is disposable: it holds no plugin state, only the last values it sent to the UI.
    std::vector<juce::RangedAudioParameter*> trackedParams;
    std::vector<float> lastSentValues;
    void snapshotParameters();

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioEditor)
};
