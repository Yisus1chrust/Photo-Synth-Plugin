/*
  ==============================================================================
    CustomLookAndFeel.h
    Modular Sound Technologies - Photo Synth
    100% Visual Styling Match to Paint A Synth Web App
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
        setColour(juce::Slider::trackColourId, juce::Colour(0xff18181b));

        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff141417));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff06b6d4));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe4e4e7));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff09090b));

        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff141417));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe4e4e7));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff27272a));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff22d3ee));

        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff141417));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe4e4e7));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff27272a));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff22d3ee));

        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff18181b));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xfff4f4f5));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff27272a));
        setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff22d3ee));
    }

    // 1. Sleek Rotary Sliders with Neon Glow Arcs
    void drawRotarySlider(
        juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider
    ) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(3.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();

        // Outer Dark Bezel / Recess
        g.setColour(juce::Colour(0xff121215));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Active LED Arc Track
        float arcRadius = radius - 3.5f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff1f242e));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path filledArc;
        filledArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);

        // Neon Glow / Accent
        juce::Colour activeColor = juce::Colour(0xff06b6d4); // Cyan default
        juce::String name = slider.getName();
        if (name.containsIgnoreCase("decay") || name.containsIgnoreCase("resonance") || name.containsIgnoreCase("depth") || name.containsIgnoreCase("phaser")) {
            activeColor = juce::Colour(0xffa855f7); // Purple
        } else if (name.containsIgnoreCase("volume") || name.containsIgnoreCase("attack") || name.containsIgnoreCase("reverb")) {
            activeColor = juce::Colour(0xffec4899); // Pink
        } else if (name.containsIgnoreCase("flanger") || name.containsIgnoreCase("chorus")) {
            activeColor = juce::Colour(0xff3b82f6); // Blue
        } else if (name.containsIgnoreCase("drive") || name.containsIgnoreCase("distortion")) {
            activeColor = juce::Colour(0xfff59e0b); // Amber
        }

        g.setColour(activeColor);
        g.strokePath(filledArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Brushed Metal Knob Face
        float knobRadius = radius - 8.0f;
        juce::ColourGradient knobGrad(
            juce::Colour(0xff27272a), center.x - knobRadius, center.y - knobRadius,
            juce::Colour(0xff0e0e11), center.x + knobRadius, center.y + knobRadius,
            false
        );
        g.setGradientFill(knobGrad);
        g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

        // White / Cyan Indicator Line
        juce::Path p;
        p.startNewSubPath(center.x, center.y - knobRadius * 0.45f);
        p.lineTo(center.x, center.y - knobRadius * 0.95f);
        p.applyTransform(juce::AffineTransform::rotation(currentAngle, center.x, center.y));

        g.setColour(juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 2. Linear Sliders (Morph Bar)
    void drawLinearSlider(
        juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float minSliderPos,
        float maxSliderPos,
        const juce::Slider::SliderStyle,
        juce::Slider&
    ) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        float trackH = 6.0f;
        float trackY = bounds.getCentreY() - trackH * 0.5f;

        // Background groove
        g.setColour(juce::Colour(0xff18181b));
        g.fillRoundedRectangle(bounds.getX(), trackY, bounds.getWidth(), trackH, 3.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(bounds.getX(), trackY, bounds.getWidth(), trackH, 3.0f, 1.0f);

        // Filled active bar up to sliderPos
        float fillW = juce::jmax(0.0f, sliderPos - bounds.getX());
        juce::ColourGradient fillGrad(
            juce::Colour(0xff06b6d4), bounds.getX(), trackY,
            juce::Colour(0xff22d3ee), bounds.getX() + bounds.getWidth(), trackY,
            false
        );
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(bounds.getX(), trackY, fillW, trackH, 3.0f);

        // Sleek thumb handle
        float thumbW = 14.0f;
        float thumbH = bounds.getHeight() * 0.65f;
        auto thumbRect = juce::Rectangle<float>(sliderPos - thumbW * 0.5f, bounds.getCentreY() - thumbH * 0.5f, thumbW, thumbH);

        g.setColour(juce::Colour(0xff09090b));
        g.fillRoundedRectangle(thumbRect, 3.0f);
        g.setColour(juce::Colour(0xff22d3ee));
        g.drawRoundedRectangle(thumbRect, 3.0f, 1.5f);

        // Notch in thumb
        g.setColour(juce::Colours::white);
        g.drawLine(sliderPos, thumbRect.getY() + 3.0f, sliderPos, thumbRect.getBottom() - 3.0f, 1.5f);
    }

    // 3. 3D VST Button Background
    void drawButtonBackground(
        juce::Graphics& g,
        juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown
    ) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour base = backgroundColour;

        if (shouldDrawButtonAsDown)
            base = base.darker(0.3f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter(0.15f);

        juce::ColourGradient grad(
            base.brighter(0.08f), bounds.getX(), bounds.getY(),
            base.darker(0.12f), bounds.getX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 4.0f);

        juce::Colour borderCol = juce::Colour(0xff27272a);
        if (button.getName().containsIgnoreCase("save") || button.getName().containsIgnoreCase("effects"))
            borderCol = juce::Colour(0xff06b6d4).withAlpha(0.6f);
        else if (shouldDrawButtonAsHighlighted)
            borderCol = juce::Colour(0xff3f3f46);

        g.setColour(borderCol);
        g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
    }

    // 4. ComboBox Styling
    void drawComboBox(
        juce::Graphics& g,
        int width, int height,
        bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH,
        juce::ComboBox& box
    ) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(0.5f);
        g.setColour(juce::Colour(0xff121215));
        g.fillRoundedRectangle(bounds, 3.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Down arrow icon
        juce::Path arrow;
        float ax = width - 14.0f;
        float ay = height * 0.5f - 2.0f;
        arrow.startNewSubPath(ax, ay);
        arrow.lineTo(ax + 4.0f, ay + 5.0f);
        arrow.lineTo(ax + 8.0f, ay);

        g.setColour(juce::Colour(0xff22d3ee));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    // 5. Toggle Button (Limiter Switch with LED)
    void drawToggleButton(
        juce::Graphics& g,
        juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown
    ) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float ledSize = 10.0f;
        float ledX = bounds.getX() + 4.0f;
        float ledY = bounds.getCentreY() - ledSize * 0.5f;

        // LED Bezel
        g.setColour(juce::Colour(0xff18181b));
        g.fillEllipse(ledX - 1.0f, ledY - 1.0f, ledSize + 2.0f, ledSize + 2.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawEllipse(ledX - 1.0f, ledY - 1.0f, ledSize + 2.0f, ledSize + 2.0f, 1.0f);

        // LED Core
        if (button.getToggleState())
        {
            g.setColour(juce::Colour(0xff10b981)); // Green LED
            g.fillEllipse(ledX, ledY, ledSize, ledSize);
            g.setColour(juce::Colour(0x6610b981));
            g.drawEllipse(ledX - 2.0f, ledY - 2.0f, ledSize + 4.0f, ledSize + 4.0f, 2.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff3f3f46)); // Dim LED
            g.fillEllipse(ledX, ledY, ledSize, ledSize);
        }

        // Text
        g.setColour(button.getToggleState() ? juce::Colour(0xfff4f4f5) : juce::Colour(0xffa1a1aa));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), (int)(ledX + ledSize + 6.0f), 0, (int)(bounds.getWidth() - ledSize - 8.0f), (int)bounds.getHeight(), juce::Justification::centredLeft);
    }
};
