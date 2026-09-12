/*
  ==============================================================================
    CustomLookAndFeel.h
    Modular Sound Technologies - Photo Synth
    100% Visual Styling Match to Paint A Synth Prototype
  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel()
    {
        // Dark metallic palette
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff09090b));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff22d3ee));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff06b6d4));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff27272a));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff18181b));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe4e4e7));
    }

    void drawRotarySlider(
        juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider
    ) override
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();

        // 1. Outer Dark Bezel / Recess
        g.setColour(juce::Colour(0xff121215));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // 2. Active LED Arc Track
        float arcRadius = radius - 4.0f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1f242e));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path filledArc;
        filledArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);

        // Neon Glow / Accent
        juce::Colour activeColor = juce::Colour(0xff06b6d4); // Cyan default
        if (slider.getName().containsIgnoreCase("decay") || slider.getName().containsIgnoreCase("resonance")) {
            activeColor = juce::Colour(0xffa855f7); // Purple
        } else if (slider.getName().containsIgnoreCase("volume") || slider.getName().containsIgnoreCase("attack")) {
            activeColor = juce::Colour(0xffec4899); // Pink
        }
        g.setColour(activeColor);
        g.strokePath(filledArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 3. Brushed Metal Knob Face
        float knobRadius = radius - 10.0f;
        juce::ColourGradient knobGrad(
            juce::Colour(0xff27272a), center.x - knobRadius, center.y - knobRadius,
            juce::Colour(0xff0e0e11), center.x + knobRadius, center.y + knobRadius,
            false
        );
        g.setGradientFill(knobGrad);
        g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

        // 4. White / Cyan Indicator Line
        juce::Path p;
        p.startNewSubPath(center.x, center.y - knobRadius * 0.45f);
        p.lineTo(center.x, center.y - knobRadius * 0.95f);
        p.applyTransform(juce::AffineTransform::rotation(currentAngle, center.x, center.y));

        g.setColour(juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
