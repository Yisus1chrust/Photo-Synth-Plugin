#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
/** Cyberpunk Custom LookAndFeel for rotary sliders and styling */
class CyberpunkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CyberpunkLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe67e35));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff141312));
        setColour (juce::Slider::thumbColourId, juce::Colour (0xffffa052));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto center = bounds.getCentre();

        // Outer Dark Bezel
        g.setColour (juce::Colour (0xff141312));
        g.fillEllipse (bounds);
        g.setColour (juce::Colour (0xff322f2b));
        g.drawEllipse (bounds, 1.5f);

        // Arc Track Background
        juce::Path trackPath;
        trackPath.addCentredArc (center.x, center.y, radius - 4.0f, radius - 4.0f, 0.0f,
                                 rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff1c1b18));
        g.strokePath (trackPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Neon Orange Glow Arc
        juce::Path valuePath;
        valuePath.addCentredArc (center.x, center.y, radius - 4.0f, radius - 4.0f, 0.0f,
                                 rotaryStartAngle, toAngle, true);
        g.setColour (juce::Colour (0xffe67e35).withAlpha (0.4f));
        g.strokePath (valuePath, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffe67e35));
        g.strokePath (valuePath, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner Dial Cap
        auto innerBounds = bounds.reduced (9.0f);
        juce::ColourGradient grad (juce::Colour (0xff2a2724), innerBounds.getX(), innerBounds.getY(),
                                   juce::Colour (0xff1c1a18), innerBounds.getRight(), innerBounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (innerBounds);
        g.setColour (juce::Colour (0xff3d3934));
        g.drawEllipse (innerBounds, 1.0f);

        // Indicator Pointer Dot
        juce::Point<float> pointerPos (
            center.x + (radius - 12.0f) * std::cos (toAngle - juce::MathConstants<float>::halfPi),
            center.y + (radius - 12.0f) * std::sin (toAngle - juce::MathConstants<float>::halfPi)
        );
        g.setColour (juce::Colour (0xffffa052));
        g.fillEllipse (pointerPos.x - 2.5f, pointerPos.y - 2.5f, 5.0f, 5.0f);
    }
};

//==============================================================================
/** Oscilloscope Display Component */
class CyberScopeComponent : public juce::Component, private juce::Timer
{
public:
    CyberScopeComponent (PhotoSynthAudioProcessor& p) : processor (p)
    {
        startTimerHz (30);
    }

    ~CyberScopeComponent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark Recessed Display Surface
        g.setColour (juce::Colour (0xff151413));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Cyber Grid Lines
        g.setColour (juce::Colour (0xff24211d));
        for (float x = bounds.getX() + 20.0f; x < bounds.getRight(); x += 25.0f)
            g.drawVerticalLine ((int) x, bounds.getY(), bounds.getBottom());
        for (float y = bounds.getY() + 20.0f; y < bounds.getBottom(); y += 25.0f)
            g.drawHorizontalLine ((int) y, bounds.getX(), bounds.getRight());

        // Outer Border
        g.setColour (juce::Colour (0xff2d2a26));
        g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

        // Live Audio Waveform Path
        juce::Path wavePath;
        const int numPts = 128;
        float points[128];
        processor.getScopeData (points, numPts);

        float midY = bounds.getCentreY();
        float halfH = bounds.getHeight() * 0.42f;

        for (int i = 0; i < numPts; ++i)
        {
            float px = bounds.getX() + (float) i * (bounds.getWidth() / (float) (numPts - 1));
            float py = midY - points[i] * halfH;
            if (i == 0)
                wavePath.startNewSubPath (px, py);
            else
                wavePath.lineTo (px, py);
        }

        // Orange Glow Waveform
        g.setColour (juce::Colour (0xffe67e35).withAlpha (0.25f));
        g.strokePath (wavePath, juce::PathStrokeType (4.0f));
        g.setColour (juce::Colour (0xffe67e35));
        g.strokePath (wavePath, juce::PathStrokeType (1.5f));

        // Corner Overlay Text
        g.setColour (juce::Colour (0xffe67e35).withAlpha (0.7f));
        g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
        g.drawText ("TIMBRE DNA OSCILLOSCOPE // REAL-TIME", getLocalBounds().reduced (8), juce::Justification::topRight);
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    PhotoSynthAudioProcessor& processor;
};

//==============================================================================
class PhotoSynthAudioEditor : public juce::AudioProcessorEditor
{
public:
    PhotoSynthAudioEditor (PhotoSynthAudioProcessor&);
    ~PhotoSynthAudioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PhotoSynthAudioProcessor& audioProcessor;
    CyberpunkLookAndFeel cyberpunkLaf;

    // Controls Panel Component
    juce::Slider colorSlider;
    juce::Slider brightnessSlider;
    juce::Slider saturationSlider;
    juce::Slider complexitySlider;
    juce::Slider masterVolSlider;

    juce::Label colorLabel;
    juce::Label brightnessLabel;
    juce::Label saturationLabel;
    juce::Label complexityLabel;
    juce::Label masterVolLabel;

    // Direct APVTS Slider Attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> colorAttach;
    std::unique_ptr<Attachment> brightnessAttach;
    std::unique_ptr<Attachment> saturationAttach;
    std::unique_ptr<Attachment> complexityAttach;
    std::unique_ptr<Attachment> masterVolAttach;

    // Real-time oscilloscope component
    CyberScopeComponent scopeComponent;

    void configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhotoSynthAudioEditor)
};
