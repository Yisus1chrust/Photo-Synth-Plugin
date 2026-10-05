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

class PhotoSynthAudioEditor : public juce::AudioProcessorEditor
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

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioEditor)
};
