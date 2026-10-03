#include "PluginEditor.h"

PhotoSynthAudioEditor::PhotoSynthAudioEditor (PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), scopeComponent (p)
{
    setLookAndFeel (&cyberpunkLaf);

    // Configure 4 Timbre DNA Knobs
    configureKnob (colorSlider, colorLabel, "COLOR");
    colorAttach = std::make_unique<Attachment> (audioProcessor.apvts, "color", colorSlider);

    configureKnob (brightnessSlider, brightnessLabel, "BRIGHTNESS");
    brightnessAttach = std::make_unique<Attachment> (audioProcessor.apvts, "brightness", brightnessSlider);

    configureKnob (saturationSlider, saturationLabel, "SATURATION");
    saturationAttach = std::make_unique<Attachment> (audioProcessor.apvts, "saturation", saturationSlider);

    configureKnob (complexitySlider, complexityLabel, "COMPLEXITY");
    complexityAttach = std::make_unique<Attachment> (audioProcessor.apvts, "complexity", complexitySlider);

    // Master Volume Knob
    configureKnob (masterVolSlider, masterVolLabel, "MASTER VOL");
    masterVolAttach = std::make_unique<Attachment> (audioProcessor.apvts, "master_vol", masterVolSlider);

    // Add Central Visual Scope
    addAndMakeVisible (scopeComponent);

    // Window size (Modern square-ish synth format)
    setSize (860, 520);
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    setLookAndFeel (nullptr);
}

void PhotoSynthAudioEditor::configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 18);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe67e35));
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffbfb7ae));
    addAndMakeVisible (label);
}

void PhotoSynthAudioEditor::paint (juce::Graphics& g)
{
    // Background `#1a1816`
    g.fillAll (juce::Colour (0xff1a1816));

    auto bounds = getLocalBounds().toFloat();

    // Top Title Bar
    auto topBar = bounds.removeFromTop (46.0f);
    g.setColour (juce::Colour (0xff1f1d1a));
    g.fillRect (topBar);
    g.setColour (juce::Colour (0xff2e2b26));
    g.drawHorizontalLine (45, topBar.getX(), topBar.getRight());

    // Cyber Brand Title
    g.setColour (juce::Colour (0xfff2ede4));
    g.setFont (juce::FontOptions (14.0f).withStyle ("Bold"));
    g.drawText ("PAINT A SYNTH", 18, 0, 200, 46, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffe67e35));
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    g.drawText ("MODULAR SOUND TECHNOLOGIES // V1.0 NATIVE", topBar.reduced (16, 0), juce::Justification::centredRight);

    // Side Controls Panel Surface `#24221f`
    auto sidePanel = bounds.removeFromLeft (260.0f).reduced (10.0f);
    g.setColour (juce::Colour (0xff24221f));
    g.fillRoundedRectangle (sidePanel, 8.0f);
    g.setColour (juce::Colour (0xff33302b));
    g.drawRoundedRectangle (sidePanel, 8.0f, 1.5f);

    // Section Header in Controls Panel
    g.setColour (juce::Colour (0xffe67e35));
    g.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    g.drawText ("TIMBRE DNA CONTROLS", (int) sidePanel.getX() + 14, (int) sidePanel.getY() + 10, 200, 20, juce::Justification::left);

    // Central Display Panel Surface `#24221f`
    auto centerPanel = bounds.reduced (10.0f);
    g.setColour (juce::Colour (0xff24221f));
    g.fillRoundedRectangle (centerPanel, 8.0f);
    g.setColour (juce::Colour (0xff33302b));
    g.drawRoundedRectangle (centerPanel, 8.0f, 1.5f);
}

void PhotoSynthAudioEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (46); // Top title bar

    // Side Controls Panel Layout
    auto sideArea = area.removeFromLeft (260).reduced (16);
    sideArea.removeFromTop (28); // Header label offset

    // 2x2 Grid for 4 Timbre DNA Knobs
    int knobSize = 92;
    int colWidth = (sideArea.getWidth() - 10) / 2;

    auto row1 = sideArea.removeFromTop (knobSize + 22);
    auto c1 = row1.removeFromLeft (colWidth);
    colorLabel.setBounds (c1.removeFromBottom (18));
    colorSlider.setBounds (c1);

    row1.removeFromLeft (10);
    brightnessLabel.setBounds (row1.removeFromBottom (18));
    brightnessSlider.setBounds (row1);

    sideArea.removeFromTop (12);

    auto row2 = sideArea.removeFromTop (knobSize + 22);
    auto c2 = row2.removeFromLeft (colWidth);
    saturationLabel.setBounds (c2.removeFromBottom (18));
    saturationSlider.setBounds (c2);

    row2.removeFromLeft (10);
    complexityLabel.setBounds (row2.removeFromBottom (18));
    complexitySlider.setBounds (row2);

    sideArea.removeFromTop (16);

    // Bottom Master Volume Knob
    auto masterArea = sideArea.removeFromBottom (85);
    masterVolLabel.setBounds (masterArea.removeFromBottom (18));
    masterVolSlider.setBounds (masterArea.withSizeKeepingCentre (75, 75));

    // Central Scope Display Layout
    auto centerArea = area.reduced (18);
    scopeComponent.setBounds (centerArea);
}
