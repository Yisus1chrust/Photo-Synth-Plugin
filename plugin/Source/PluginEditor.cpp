/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// -----------------------------------------------------------------------------
// RadialGaugeComponent Implementation
// -----------------------------------------------------------------------------
RadialGaugeComponent::RadialGaugeComponent(const juce::String& labelText, juce::Colour ringColor)
    : label(labelText), color(ringColor) {}

void RadialGaugeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight() - 16.0f) / 2.0f;
    auto center = juce::Point<float>(bounds.getCentreX(), bounds.getY() + radius + 2.0f);

    // Dark background ring
    g.setColour(juce::Colour(0xff18181b));
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

    // Active Value Arc
    float startAngle = juce::MathConstants<float>::pi * 0.75f;
    float endAngle = startAngle + targetValue * (juce::MathConstants<float>::pi * 1.5f);
    juce::Path arc;
    arc.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f, startAngle, endAngle, true);
    g.setColour(color);
    g.strokePath(arc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value percentage text
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText(juce::String(std::round(targetValue * 100.0f)) + "%",
               juce::Rectangle<float>(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f),
               juce::Justification::centred);

    // Label below
    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(label, getLocalBounds().removeFromBottom(14), juce::Justification::centred);
}

// -----------------------------------------------------------------------------
// OscilloscopeComponent Implementation
// -----------------------------------------------------------------------------
void OscilloscopeComponent::pushData(const float* samples, int numSamples)
{
    int copyCount = juce::jmin(numSamples, (int)waveformData.size());
    for (int i = 0; i < copyCount; ++i) {
        waveformData[i] = samples[i];
    }
    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b));
    g.setColour(juce::Colour(0xff27272a));
    g.drawRect(getLocalBounds(), 1);

    // Cyan Neon Waveform
    juce::Path wavePath;
    float w = (float)getWidth();
    float h = (float)getHeight();
    float midY = h * 0.5f;

    wavePath.startNewSubPath(0.0f, midY);
    for (size_t i = 0; i < waveformData.size(); ++i) {
        float x = (float)i / (float)(waveformData.size() - 1) * w;
        float y = midY - waveformData[i] * (h * 0.42f);
        wavePath.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff06b6d4).withAlpha(0.85f));
    g.strokePath(wavePath, juce::PathStrokeType(1.8f));
}

// -----------------------------------------------------------------------------
// PhotoSynthAudioEditor Implementation
// -----------------------------------------------------------------------------
PhotoSynthAudioEditor::PhotoSynthAudioEditor (PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&customLookAndFeel);
    setSize (1140, 760);

    // Add Gauges
    addAndMakeVisible(colorGauge);
    addAndMakeVisible(brightnessGauge);
    addAndMakeVisible(saturationGauge);
    addAndMakeVisible(complexityGauge);

    // Center Components
    addAndMakeVisible(oscilloscope);
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setAvailableRange(36, 84); // C2 to C6
    keyboardComponent.setKeyWidth(18.0f);

    // Setup & Add Knobs
    auto setupRotary = [this](juce::Slider& slider, const juce::String& name) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        slider.setName(name);
        addAndMakeVisible(slider);
    };

    setupRotary(cutoffKnob, "Cutoff");
    setupRotary(resKnob, "Resonance");
    setupRotary(lfoRateKnob, "LFO Rate");
    setupRotary(lfoDepthKnob, "LFO Depth");
    setupRotary(volKnob, "Volume");
    setupRotary(attackKnob, "Attack");
    setupRotary(decayKnob, "Decay");
    setupRotary(sustainKnob, "Sustain");
    setupRotary(releaseKnob, "Release");
    setupRotary(driveKnob, "Drive");

    addAndMakeVisible(limiterButton);

    // Top Bar Controls
    morphSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(morphSlider);

    presetBox.addItem("01. Analog Saw Stack", 1);
    presetBox.addItem("02. Digital FM Glass", 2);
    presetBox.addItem("03. Modular Dark Drone", 3);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(presetBox);

    addAndMakeVisible(saveButton);
    addAndMakeVisible(deepDiveButton);

    // Bind APVTS Attachments
    cutoffAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff", cutoffKnob);
    resAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "resonance", resKnob);
    lfoRateAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "lfoRate", lfoRateKnob);
    lfoDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "lfoDepth", lfoDepthKnob);
    volAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "volume", volKnob);
    attackAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "attack", attackKnob);
    decayAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "decay", decayKnob);
    sustainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "sustain", sustainKnob);
    releaseAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "release", releaseKnob);
    driveAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "drive", driveKnob);
    morphAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "morph", morphSlider);
    limiterAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "limiter", limiterButton);

    startTimerHz(30); // 30 FPS UI Update
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PhotoSynthAudioEditor::timerCallback()
{
    // Update Gauges from current analysis
    auto res = audioProcessor.getLatestAnalysis();
    colorGauge.setValue(res.timbreDna);
    brightnessGauge.setValue(res.brightness);
    saturationGauge.setValue(res.saturation);
    complexityGauge.setValue(res.complexity);

    // Refresh oscilloscope
    float tempBuffer[256];
    audioProcessor.getScopeData(tempBuffer, 256);
    oscilloscope.pushData(tempBuffer, 256);
}

bool PhotoSynthAudioEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() > 0 && (
        files[0].endsWithIgnoreCase(".png") ||
        files[0].endsWithIgnoreCase(".jpg") ||
        files[0].endsWithIgnoreCase(".jpeg") ||
        files[0].endsWithIgnoreCase(".webp")
    );
}

void PhotoSynthAudioEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0) {
        juce::File imageFile(files[0]);
        currentLoadedImage = juce::ImageFileFormat::loadFrom(imageFile);
        if (!currentLoadedImage.isNull()) {
            audioProcessor.loadSlotAImage(currentLoadedImage);
            repaint();
        }
    }
}

void PhotoSynthAudioEditor::paint (juce::Graphics& g)
{
    // Deep dark background chassis
    g.fillAll (juce::Colour (0xff09090b));

    // Top Title Bar
    auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), 56);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(headerBounds);
    g.setColour(juce::Colour(0xff27272a));
    g.drawHorizontalLine(56, 0.0f, (float)getWidth());

    // MST Logo & Title
    g.setColour(juce::Colour(0xffe4e4e7));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("PAINT A SYNTH", 24, 12, 200, 32, juce::Justification::left);

    g.setColour(juce::Colour(0xff71717a));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("MODULAR SOUND TECHNOLOGIES", 24, 38, 220, 14, juce::Justification::left);

    // Central Image Drop Area
    auto dropBounds = juce::Rectangle<int>(270, 70, 440, 340);
    g.setColour(juce::Colour(0xff121215));
    g.fillRect(dropBounds);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRect(dropBounds.toFloat(), 1.5f);

    if (!currentLoadedImage.isNull()) {
        g.drawImage(currentLoadedImage, dropBounds.toFloat(), juce::RectanglePlacement::centred);
    } else {
        g.setColour(juce::Colour(0xff71717a));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("DRAG & DROP IMAGE HERE (SLOT A)", dropBounds, juce::Justification::centred);
    }

    // Era label badge
    auto analysis = audioProcessor.getLatestAnalysis();
    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("ERA: " + analysis.temporalEra.eraLabel, 270, 416, 440, 20, juce::Justification::centred);
}

void PhotoSynthAudioEditor::resized()
{
    // Header Bar Elements
    morphSlider.setBounds(340, 14, 280, 28);
    presetBox.setBounds(650, 14, 160, 28);
    saveButton.setBounds(820, 14, 70, 28);
    deepDiveButton.setBounds(900, 14, 90, 28);

    // Left Column (Gauges): X = 20, W = 220
    int gaugeSize = 100;
    colorGauge.setBounds(30, 80, gaugeSize, gaugeSize + 20);
    brightnessGauge.setBounds(140, 80, gaugeSize, gaugeSize + 20);
    saturationGauge.setBounds(30, 220, gaugeSize, gaugeSize + 20);
    complexityGauge.setBounds(140, 220, gaugeSize, gaugeSize + 20);

    // Center Column: Oscilloscope & Keyboard
    oscilloscope.setBounds(270, 440, 440, 110);
    keyboardComponent.setBounds(270, 565, 440, 120);

    // Right Column (Knobs Grid): X = 740, W = 360
    int rx = 740;
    int kw = 70;
    int kh = 85;
    int gapX = 15;
    int gapY = 15;

    // Row 1
    cutoffKnob.setBounds(rx, 80, kw, kh);
    resKnob.setBounds(rx + kw + gapX, 80, kw, kh);
    lfoRateKnob.setBounds(rx + (kw + gapX) * 2, 80, kw, kh);
    lfoDepthKnob.setBounds(rx + (kw + gapX) * 3, 80, kw, kh);

    // Row 2
    volKnob.setBounds(rx, 80 + kh + gapY, kw, kh);
    attackKnob.setBounds(rx + kw + gapX, 80 + kh + gapY, kw, kh);
    decayKnob.setBounds(rx + (kw + gapX) * 2, 80 + kh + gapY, kw, kh);
    sustainKnob.setBounds(rx + (kw + gapX) * 3, 80 + kh + gapY, kw, kh);

    // Row 3
    releaseKnob.setBounds(rx, 80 + (kh + gapY) * 2, kw, kh);
    driveKnob.setBounds(rx + kw + gapX, 80 + (kh + gapY) * 2, kw, kh);
    limiterButton.setBounds(rx + (kw + gapX) * 2, 80 + (kh + gapY) * 2 + 20, 100, 30);
}
