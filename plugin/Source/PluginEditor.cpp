/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Photo Synth
    100% 1:1 Accurate Layout & Features matching Web Prototype
  ==============================================================================
*/

#include "PluginEditor.h"

// -----------------------------------------------------------------------------
// 1. Radial Gauge Component
// -----------------------------------------------------------------------------
RadialGaugeComponent::RadialGaugeComponent(const juce::String& labelText, juce::Colour ringColor)
    : label(labelText), color(ringColor)
{
}

void RadialGaugeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    auto center = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.48f;

    // Outer dark circular card
    g.setColour(juce::Colour(0xff121215));
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

    // Track arc
    float arcRadius = radius - 5.0f;
    float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    float endAngle = juce::MathConstants<float>::pi * 0.75f;

    juce::Path trackPath;
    trackPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff1f242e));
    g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Filled arc
    float currentAngle = startAngle + juce::jlimit(0.0f, 1.0f, targetValue) * (endAngle - startAngle);
    juce::Path filledPath;
    filledPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, startAngle, currentAngle, true);
    g.setColour(color);
    g.strokePath(filledPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value percentage text in center
    g.setColour(juce::Colour(0xfff4f4f5));
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    int pct = juce::roundToInt(targetValue * 100.0f);
    g.drawText(juce::String(pct) + "%", (int)(center.x - radius), (int)(center.y - 12.0f), (int)(radius * 2.0f), 16, juce::Justification::centred);

    // Label under percentage
    g.setColour(color.brighter(0.2f));
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(label, (int)(center.x - radius), (int)(center.y + 4.0f), (int)(radius * 2.0f), 12, juce::Justification::centred);
}

// -----------------------------------------------------------------------------
// 2. Oscilloscope Component
// -----------------------------------------------------------------------------
void OscilloscopeComponent::pushData(const float* samples, int numSamples)
{
    int copyCount = juce::jmin((int)waveformData.size(), numSamples);
    std::copy(samples, samples + copyCount, waveformData.begin());
    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark inset background
    g.setColour(juce::Colour(0xff09090b));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Subtle center grid line
    g.setColour(juce::Colour(0x2222d3ee));
    g.drawHorizontalLine((int)bounds.getCentreY(), bounds.getX(), bounds.getRight());

    // Waveform path
    juce::Path wavePath;
    float midY = bounds.getCentreY();
    float step = bounds.getWidth() / (float)waveformData.size();

    wavePath.startNewSubPath(bounds.getX(), midY);
    for (size_t i = 0; i < waveformData.size(); ++i)
    {
        float x = bounds.getX() + i * step;
        float y = midY - waveformData[i] * (bounds.getHeight() * 0.42f);
        wavePath.lineTo(x, juce::jlimit(bounds.getY(), bounds.getBottom(), y));
    }

    // Glow stroke
    g.setColour(juce::Colour(0x4406b6d4));
    g.strokePath(wavePath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff22d3ee));
    g.strokePath(wavePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// -----------------------------------------------------------------------------
// 3. Virtual Piano Keyboard Component
// -----------------------------------------------------------------------------
VirtualPianoKeyboard::VirtualPianoKeyboard(PhotoSynthAudioProcessor& proc)
    : processor(proc)
{
    setWantsKeyboardFocus(true);
}

VirtualPianoKeyboard::~VirtualPianoKeyboard()
{
    processor.allNotesOff();
}

bool VirtualPianoKeyboard::isBlackKey(int midiNote)
{
    int noteInOctave = midiNote % 12;
    return (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
}

void VirtualPianoKeyboard::resized()
{
    keys.clear();
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() <= 10.0f || bounds.getHeight() <= 10.0f) return;

    const int startNote = 48; // C3
    const int endNote   = 72; // C5 (25 keys total, 15 white keys)

    int whiteCount = 0;
    for (int n = startNote; n <= endNote; ++n) {
        if (!isBlackKey(n)) whiteCount++;
    }
    if (whiteCount == 0) return;

    float whiteW = bounds.getWidth() / (float)whiteCount;
    float blackW = whiteW * 0.62f;
    float blackH = bounds.getHeight() * 0.62f;

    auto getKeyChar = [](int midi) -> juce::String {
        switch (midi) {
            case 48: return "A";
            case 49: return "W";
            case 50: return "S";
            case 51: return "E";
            case 52: return "D";
            case 53: return "F";
            case 54: return "T";
            case 55: return "G";
            case 56: return "Y";
            case 57: return "H";
            case 58: return "U";
            case 59: return "J";
            case 60: return "K";
            case 61: return "O";
            case 62: return "L";
            case 63: return "P";
            case 64: return ";";
            case 65: return "'";
            default: return {};
        }
    };

    // Pass 1: white keys
    int whiteIdx = 0;
    for (int n = startNote; n <= endNote; ++n)
    {
        if (!isBlackKey(n))
        {
            auto r = juce::Rectangle<float>((float)whiteIdx * whiteW, 0.0f, whiteW - 1.0f, bounds.getHeight());
            keys.push_back({ n, false, r, getKeyChar(n) });
            whiteIdx++;
        }
    }

    // Pass 2: black keys
    whiteIdx = 0;
    for (int n = startNote; n <= endNote; ++n)
    {
        if (isBlackKey(n))
        {
            float bx = (float)whiteIdx * whiteW - (blackW * 0.5f);
            auto r = juce::Rectangle<float>(bx, 0.0f, blackW, blackH);
            keys.push_back({ n, true, r, getKeyChar(n) });
        }
        else
        {
            whiteIdx++;
        }
    }
}

int VirtualPianoKeyboard::getNoteAtPosition(juce::Point<int> pos)
{
    auto fpos = pos.toFloat();
    // 1. Black keys first (top layer)
    for (const auto& k : keys) {
        if (k.isBlack && k.bounds.contains(fpos))
            return k.note;
    }
    // 2. White keys
    for (const auto& k : keys) {
        if (!k.isBlack && k.bounds.contains(fpos))
            return k.note;
    }
    return -1;
}

void VirtualPianoKeyboard::triggerNoteOn(int midiNote, float vel)
{
    if (activeNotes.find(midiNote) == activeNotes.end())
    {
        activeNotes.insert(midiNote);
        processor.noteOn(midiNote, vel);
        repaint();
    }
}

void VirtualPianoKeyboard::triggerNoteOff(int midiNote)
{
    if (activeNotes.find(midiNote) != activeNotes.end())
    {
        activeNotes.erase(midiNote);
        processor.noteOff(midiNote);
        repaint();
    }
}

void VirtualPianoKeyboard::mouseDown(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note != -1)
    {
        currentMouseNote = note;
        triggerNoteOn(note, 0.85f);
    }
}

void VirtualPianoKeyboard::mouseDrag(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note != currentMouseNote)
    {
        if (currentMouseNote != -1)
            triggerNoteOff(currentMouseNote);

        currentMouseNote = note;
        if (currentMouseNote != -1)
            triggerNoteOn(currentMouseNote, 0.85f);
    }
}

void VirtualPianoKeyboard::mouseUp(const juce::MouseEvent&)
{
    if (currentMouseNote != -1)
    {
        triggerNoteOff(currentMouseNote);
        currentMouseNote = -1;
    }
}

int VirtualPianoKeyboard::keyPressToMidi(const juce::KeyPress& key)
{
    auto c = std::tolower(key.getTextCharacter());
    switch (c) {
        case 'a': return 48; // C3
        case 'w': return 49; // C#3
        case 's': return 50; // D3
        case 'e': return 51; // D#3
        case 'd': return 52; // E3
        case 'f': return 53; // F3
        case 't': return 54; // F#3
        case 'g': return 55; // G3
        case 'y': return 56; // G#3
        case 'h': return 57; // A3
        case 'u': return 58; // A#3
        case 'j': return 59; // B3
        case 'k': return 60; // C4
        case 'o': return 61; // C#4
        case 'l': return 62; // D4
        case 'p': return 63; // D#4
        case ';': return 64; // E4
        case '\'': return 65; // F4
        default: break;
    }
    return -1;
}

bool VirtualPianoKeyboard::keyPressed(const juce::KeyPress& key)
{
    int midi = keyPressToMidi(key);
    if (midi != -1) {
        triggerNoteOn(midi, 0.85f);
        return true;
    }
    return false;
}

bool VirtualPianoKeyboard::keyStateChanged(bool)
{
    std::vector<int> toRemove;
    for (int n : activeNotes) {
        if (n != currentMouseNote) {
            bool stillDown = false;
            if (n == 48 && juce::KeyPress::isKeyCurrentlyDown('a')) stillDown = true;
            else if (n == 49 && juce::KeyPress::isKeyCurrentlyDown('w')) stillDown = true;
            else if (n == 50 && juce::KeyPress::isKeyCurrentlyDown('s')) stillDown = true;
            else if (n == 51 && juce::KeyPress::isKeyCurrentlyDown('e')) stillDown = true;
            else if (n == 52 && juce::KeyPress::isKeyCurrentlyDown('d')) stillDown = true;
            else if (n == 53 && juce::KeyPress::isKeyCurrentlyDown('f')) stillDown = true;
            else if (n == 54 && juce::KeyPress::isKeyCurrentlyDown('t')) stillDown = true;
            else if (n == 55 && juce::KeyPress::isKeyCurrentlyDown('g')) stillDown = true;
            else if (n == 56 && juce::KeyPress::isKeyCurrentlyDown('y')) stillDown = true;
            else if (n == 57 && juce::KeyPress::isKeyCurrentlyDown('h')) stillDown = true;
            else if (n == 58 && juce::KeyPress::isKeyCurrentlyDown('u')) stillDown = true;
            else if (n == 59 && juce::KeyPress::isKeyCurrentlyDown('j')) stillDown = true;
            else if (n == 60 && juce::KeyPress::isKeyCurrentlyDown('k')) stillDown = true;
            else if (n == 61 && juce::KeyPress::isKeyCurrentlyDown('o')) stillDown = true;
            else if (n == 62 && juce::KeyPress::isKeyCurrentlyDown('l')) stillDown = true;
            else if (n == 63 && juce::KeyPress::isKeyCurrentlyDown('p')) stillDown = true;

            if (!stillDown) {
                toRemove.push_back(n);
            }
        }
    }
    for (int n : toRemove) {
        triggerNoteOff(n);
    }
    return true;
}

void VirtualPianoKeyboard::paint(juce::Graphics& g)
{
    // 1. Draw White Keys
    for (const auto& k : keys)
    {
        if (!k.isBlack)
        {
            bool isPressed = (activeNotes.count(k.note) > 0);
            if (isPressed)
            {
                g.setColour(juce::Colour(0xffeab308)); // Bright amber active
            }
            else
            {
                juce::ColourGradient grad(
                    juce::Colour(0xff27272a), k.bounds.getX(), k.bounds.getY(),
                    juce::Colour(0xff18181b), k.bounds.getX(), k.bounds.getBottom(),
                    false
                );
                g.setGradientFill(grad);
            }

            g.fillRoundedRectangle(k.bounds, 2.0f);
            g.setColour(juce::Colour(0xff3f3f46));
            g.drawRoundedRectangle(k.bounds, 2.0f, 1.0f);

            if (k.keyChar.isNotEmpty()) {
                g.setColour(isPressed ? juce::Colour(0xff09090b) : juce::Colour(0xff71717a));
                g.setFont(juce::Font(9.0f, juce::Font::bold));
                g.drawText(k.keyChar, k.bounds.reduced(2.0f).removeFromBottom(16.0f), juce::Justification::centred);
            }
        }
    }

    // 2. Draw Black Keys
    for (const auto& k : keys)
    {
        if (k.isBlack)
        {
            bool isPressed = (activeNotes.count(k.note) > 0);
            if (isPressed)
            {
                g.setColour(juce::Colour(0xfff59e0b)); // Amber active
            }
            else
            {
                juce::ColourGradient grad(
                    juce::Colour(0xff18181b), k.bounds.getX(), k.bounds.getY(),
                    juce::Colour(0xff09090b), k.bounds.getX(), k.bounds.getBottom(),
                    false
                );
                g.setGradientFill(grad);
            }

            g.fillRoundedRectangle(k.bounds, 2.0f);
            g.setColour(juce::Colour(0xff27272a));
            g.drawRoundedRectangle(k.bounds, 2.0f, 1.0f);

            if (k.keyChar.isNotEmpty()) {
                g.setColour(isPressed ? juce::Colour(0xff09090b) : juce::Colour(0xffa1a1aa));
                g.setFont(juce::Font(8.0f, juce::Font::bold));
                g.drawText(k.keyChar, k.bounds.reduced(1.0f).removeFromBottom(14.0f), juce::Justification::centred);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// 4. MST Logo Component
// -----------------------------------------------------------------------------
LogoComponent::LogoComponent()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void LogoComponent::mouseUp(const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

void LogoComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void LogoComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}

void LogoComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark bezel
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(bounds);
    g.setColour(isHovered ? juce::Colour(0xffeab308) : juce::Colour(0xff27272a));
    g.drawRect(bounds, isHovered ? 2.0f : 1.5f);

    // Inner card with subtle radial highlight
    auto inner = bounds.reduced(5.0f);
    juce::ColourGradient rad(
        isHovered ? juce::Colour(0x55f59e0b) : juce::Colour(0x33ffffff), inner.getCentreX(), inner.getCentreY(),
        juce::Colour(0x05ffffff), inner.getX(), inner.getY(),
        true
    );
    g.setGradientFill(rad);
    g.fillRoundedRectangle(inner, 4.0f);
    g.setColour(isHovered ? juce::Colour(0xaaeab308) : juce::Colour(0x44ffffff));
    g.drawRoundedRectangle(inner, 4.0f, 1.0f);

    // Vector 'M' emblem with soundwave bars
    float cx = inner.getCentreX();
    float cy = inner.getCentreY() - 6.0f;

    g.setColour(isHovered ? juce::Colour(0xffeab308) : juce::Colour(0xff22d3ee));
    juce::Path mPath;
    mPath.startNewSubPath(cx - 16.0f, cy + 10.0f);
    mPath.lineTo(cx - 16.0f, cy - 10.0f);
    mPath.lineTo(cx - 8.0f, cy + 2.0f);
    mPath.lineTo(cx, cy - 10.0f);
    mPath.lineTo(cx + 8.0f, cy + 2.0f);
    mPath.lineTo(cx + 16.0f, cy - 10.0f);
    mPath.lineTo(cx + 16.0f, cy + 10.0f);
    g.strokePath(mPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Subtitle text
    g.setColour(isHovered ? juce::Colour(0xffeab308) : juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(7.0f, juce::Font::bold));
    g.drawText("MODULAR SOUND", 0, (int)(inner.getBottom() - 17.0f), (int)bounds.getWidth(), 8, juce::Justification::centred);
    g.drawText("TECHNOLOGIES", 0, (int)(inner.getBottom() - 10.0f), (int)bounds.getWidth(), 8, juce::Justification::centred);

    if (isHovered) {
        g.setColour(juce::Colour(0xffeab308));
        g.setFont(juce::Font(6.5f, juce::Font::bold));
        g.drawText("[ EXPORT VST ]", 0, (int)(inner.getBottom() - 3.0f), (int)bounds.getWidth(), 7, juce::Justification::centred);
    }
}

// -----------------------------------------------------------------------------
// 5. Effects Modal Overlay Component
// -----------------------------------------------------------------------------
EffectsModalComponent::EffectsModalComponent(PhotoSynthAudioProcessor& proc, SynthLookAndFeel&, std::function<void()> onClose)
    : processor(proc), closeCallback(onClose)
{
    addAndMakeVisible(closeBtn);
    addAndMakeVisible(xBtn);
    addAndMakeVisible(allOnBtn);
    addAndMakeVisible(allOffBtn);

    closeBtn.onClick = [this] { if (closeCallback) closeCallback(); };
    xBtn.onClick = [this] { if (closeCallback) closeCallback(); };

    allOnBtn.onClick = [this] {
        processor.apvts.getParameter("delayEnabled")->setValueNotifyingHost(1.0f);
        processor.apvts.getParameter("reverbEnabled")->setValueNotifyingHost(1.0f);
        processor.apvts.getParameter("chorusEnabled")->setValueNotifyingHost(1.0f);
        processor.apvts.getParameter("phaserEnabled")->setValueNotifyingHost(1.0f);
        processor.apvts.getParameter("flangerEnabled")->setValueNotifyingHost(1.0f);
        processor.apvts.getParameter("distortionEnabled")->setValueNotifyingHost(1.0f);
    };

    allOffBtn.onClick = [this] {
        processor.apvts.getParameter("delayEnabled")->setValueNotifyingHost(0.0f);
        processor.apvts.getParameter("reverbEnabled")->setValueNotifyingHost(0.0f);
        processor.apvts.getParameter("chorusEnabled")->setValueNotifyingHost(0.0f);
        processor.apvts.getParameter("phaserEnabled")->setValueNotifyingHost(0.0f);
        processor.apvts.getParameter("flangerEnabled")->setValueNotifyingHost(0.0f);
        processor.apvts.getParameter("distortionEnabled")->setValueNotifyingHost(0.0f);
    };

    // Helper to configure effect knob
    auto setupKnob = [this](juce::Slider& knob, const juce::String& paramID, const juce::String& name) {
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setName(name);
        addAndMakeVisible(knob);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, paramID, knob));
    };

    auto setupToggle = [this](juce::ToggleButton& toggle, const juce::String& paramID) {
        addAndMakeVisible(toggle);
        buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, paramID, toggle));
    };

    // 1. Delay
    setupToggle(delayToggle, "delayEnabled");
    setupKnob(delayTimeKnob, "delayTime", "Delay Time");
    setupKnob(delayFbKnob, "delayFeedback", "Delay FB");
    setupKnob(delayMixKnob, "delayMix", "Delay Mix");

    // 2. Reverb
    setupToggle(reverbToggle, "reverbEnabled");
    setupKnob(reverbSizeKnob, "reverbSize", "Reverb Size");
    setupKnob(reverbDecayKnob, "reverbDecay", "Reverb Decay");
    setupKnob(reverbMixKnob, "reverbMix", "Reverb Mix");

    // 3. Chorus
    setupToggle(chorusToggle, "chorusEnabled");
    setupKnob(chorusRateKnob, "chorusRate", "Chorus Rate");
    setupKnob(chorusDepthKnob, "chorusDepth", "Chorus Depth");
    setupKnob(chorusMixKnob, "chorusMix", "Chorus Mix");

    // 4. Phaser
    setupToggle(phaserToggle, "phaserEnabled");
    setupKnob(phaserRateKnob, "phaserRate", "Phaser Rate");
    setupKnob(phaserDepthKnob, "phaserDepth", "Phaser Depth");
    setupKnob(phaserFbKnob, "phaserFeedback", "Phaser FB");

    // 5. Flanger
    setupToggle(flangerToggle, "flangerEnabled");
    setupKnob(flangerRateKnob, "flangerRate", "Flanger Rate");
    setupKnob(flangerDepthKnob, "flangerDepth", "Flanger Depth");
    setupKnob(flangerMixKnob, "flangerMix", "Flanger Mix");

    // 6. Distortion
    setupToggle(distToggle, "distortionEnabled");
    setupKnob(distDriveKnob, "distortionDrive", "Dist Drive");
    setupKnob(distMixKnob, "distortionMix", "Dist Mix");
}

EffectsModalComponent::~EffectsModalComponent() {}

void EffectsModalComponent::paint(juce::Graphics& g)
{
    // Dark modal backdrop overlay
    g.setColour(juce::Colour(0xdd09090b));
    g.fillAll();

    // Center dialog card
    auto card = getLocalBounds().toFloat().reduced(40.0f, 30.0f);
    g.setColour(juce::Colour(0xff121215));
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(card, 8.0f, 1.5f);

    // Header title
    g.setColour(juce::Colour(0xfff4f4f5));
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("EFFECTS RACK", (int)card.getX() + 20, (int)card.getY() + 16, 200, 24, juce::Justification::centredLeft);

    // Subtitle
    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("PICTURE AWARE PARAMETERS: ACTIVE", (int)card.getX() + 20, (int)card.getBottom() - 32, 280, 20, juce::Justification::centredLeft);
}

void EffectsModalComponent::resized()
{
    auto card = getLocalBounds().reduced(40, 30);

    xBtn.setBounds(card.getRight() - 36, card.getY() + 14, 24, 24);
    allOffBtn.setBounds(card.getRight() - 110, card.getY() + 14, 65, 24);
    allOnBtn.setBounds(card.getRight() - 180, card.getY() + 14, 65, 24);
    closeBtn.setBounds(card.getRight() - 90, card.getBottom() - 34, 75, 24);

    // Grid of 6 cards inside card bounds
    auto gridArea = card.reduced(20, 50);
    int cols = 3;
    int rows = 2;
    int cellW = gridArea.getWidth() / cols;
    int cellH = gridArea.getHeight() / rows;

    auto placeModule = [&](int row, int col, juce::ToggleButton& toggle, juce::Slider& s1, juce::Slider& s2, juce::Slider* s3) {
        int x = gridArea.getX() + col * cellW;
        int y = gridArea.getY() + row * cellH;
        toggle.setBounds(x + 10, y + 10, cellW - 20, 24);
        int knobSize = 44;
        s1.setBounds(x + 10, y + 42, knobSize, knobSize);
        s2.setBounds(x + 10 + knobSize + 10, y + 42, knobSize, knobSize);
        if (s3)
            s3->setBounds(x + 10 + (knobSize + 10) * 2, y + 42, knobSize, knobSize);
    };

    placeModule(0, 0, delayToggle, delayTimeKnob, delayFbKnob, &delayMixKnob);
    placeModule(0, 1, reverbToggle, reverbSizeKnob, reverbDecayKnob, &reverbMixKnob);
    placeModule(0, 2, chorusToggle, chorusRateKnob, chorusDepthKnob, &chorusMixKnob);
    placeModule(1, 0, phaserToggle, phaserRateKnob, phaserDepthKnob, &phaserFbKnob);
    placeModule(1, 1, flangerToggle, flangerRateKnob, flangerDepthKnob, &flangerMixKnob);
    placeModule(1, 2, distToggle, distDriveKnob, distMixKnob, nullptr);
}

// -----------------------------------------------------------------------------
// 6. Save Preset Modal Overlay Component
// -----------------------------------------------------------------------------
SavePresetModalComponent::SavePresetModalComponent(PhotoSynthAudioProcessor& proc, std::function<void()> onClose, std::function<void(const juce::String&)> onSaved)
    : processor(proc), closeCallback(onClose), saveCallback(onSaved)
{
    addAndMakeVisible(nameInput);
    addAndMakeVisible(descInput);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(cancelBtn);

    nameInput.setText("Preset 01: My Patch");
    descInput.setText("Custom image timbre synthesis patch");

    cancelBtn.onClick = [this] { if (closeCallback) closeCallback(); };
    saveBtn.onClick = [this] {
        juce::String pName = nameInput.getText().trim();
        if (pName.isNotEmpty()) {
            processor.saveCurrentPreset(pName, descInput.getText());
            if (saveCallback) saveCallback(pName);
        }
        if (closeCallback) closeCallback();
    };
}

void SavePresetModalComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xdd09090b));
    g.fillAll();

    auto card = getLocalBounds().toFloat().withSizeKeepingCentre(420.0f, 260.0f);
    g.setColour(juce::Colour(0xff121215));
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(card, 8.0f, 1.5f);

    g.setColour(juce::Colour(0xfff4f4f5));
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("SAVE FULL PRESET", (int)card.getX() + 24, (int)card.getY() + 18, 300, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("PRESET NAME", (int)card.getX() + 24, (int)card.getY() + 48, 200, 14, juce::Justification::centredLeft);
    g.drawText("DESCRIPTION (OPTIONAL)", (int)card.getX() + 24, (int)card.getY() + 112, 200, 14, juce::Justification::centredLeft);
}

void SavePresetModalComponent::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre(420, 260);
    nameInput.setBounds(card.getX() + 24, card.getY() + 66, card.getWidth() - 48, 30);
    descInput.setBounds(card.getX() + 24, card.getY() + 130, card.getWidth() - 48, 48);
    cancelBtn.setBounds(card.getRight() - 190, card.getBottom() - 44, 75, 26);
    saveBtn.setBounds(card.getRight() - 105, card.getBottom() - 44, 85, 26);
}

// -----------------------------------------------------------------------------
// 7. Deep Dive Modal Overlay Component
// -----------------------------------------------------------------------------
DeepDiveModalComponent::DeepDiveModalComponent(PhotoSynthAudioProcessor& proc, std::function<void()> onClose)
    : processor(proc), closeCallback(onClose)
{
    addAndMakeVisible(closeBtn);
    closeBtn.onClick = [this] { if (closeCallback) closeCallback(); };
}

void DeepDiveModalComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xdd09090b));
    g.fillAll();

    auto card = getLocalBounds().toFloat().withSizeKeepingCentre(500.0f, 380.0f);
    g.setColour(juce::Colour(0xff121215));
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(card, 8.0f, 1.5f);

    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("IMAGE DNA TELEMETRY (DEEP DIVE)", (int)card.getX() + 24, (int)card.getY() + 18, 380, 20, juce::Justification::centredLeft);

    auto analysis = processor.getLatestAnalysis();

    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffe4e4e7));

    int startY = (int)card.getY() + 55;
    auto drawRow = [&](const juce::String& key, const juce::String& val, int y) {
        g.setColour(juce::Colour(0xffa1a1aa));
        g.drawText(key, (int)card.getX() + 24, y, 200, 18, juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff22d3ee));
        g.drawText(val, (int)card.getX() + 220, y, 220, 18, juce::Justification::centredLeft);
    };

    drawRow("Slot A Source:", processor.getSlotAName(), startY);
    drawRow("Slot B Source:", processor.getSlotBName(), startY + 24);
    drawRow("Crossfade Morph Value:", juce::String(*processor.apvts.getRawParameterValue("morph"), 3), startY + 48);
    drawRow("Calculated Timbre DNA:", juce::String(analysis.timbreDna * 100.0f, 1) + "%", startY + 72);
    drawRow("Brightness Luma:", juce::String(analysis.brightness * 100.0f, 1) + "%", startY + 96);
    drawRow("Color Saturation:", juce::String(analysis.saturation * 100.0f, 1) + "%", startY + 120);
    drawRow("Edge Density (Complexity):", juce::String(analysis.complexity * 100.0f, 1) + "%", startY + 144);
    drawRow("Temporal Era Mapping:", analysis.temporalEra.eraLabel, startY + 168);
    drawRow("Engine Archetype:", analysis.temporalEra.architecture, startY + 192);
    drawRow("Synthesis Model:", photosynth::toEngineParamText(analysis.archetype), startY + 216);
}

void DeepDiveModalComponent::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre(500, 380);
    closeBtn.setBounds(card.getRight() - 95, card.getBottom() - 40, 75, 26);
}

// -----------------------------------------------------------------------------
// 8. Export VST / AU Modal Overlay Component
// -----------------------------------------------------------------------------
ExportModalComponent::ExportModalComponent(PhotoSynthAudioProcessor& proc, std::function<void()> onClose)
    : processor(proc), closeCallback(onClose)
{
    addAndMakeVisible(closeBtn);
    closeBtn.onClick = [this] { if (closeCallback) closeCallback(); };
}

void ExportModalComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xdd09090b));
    g.fillAll();

    auto card = getLocalBounds().toFloat().withSizeKeepingCentre(580.0f, 420.0f);
    g.setColour(juce::Colour(0xff121215));
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(juce::Colour(0xfff59e0b));
    g.drawRoundedRectangle(card, 8.0f, 1.5f);

    // Title
    g.setColour(juce::Colour(0xfff59e0b));
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("MODULAR SOUND TECHNOLOGIES - VST3 & AU EXPORT", (int)card.getX() + 24, (int)card.getY() + 18, 520, 22, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(10.5f, juce::Font::plain));
    g.drawText("This plugin codebase is built with JUCE 7 and CMake for 1:1 cross-platform parity.", (int)card.getX() + 24, (int)card.getY() + 45, 520, 16, juce::Justification::centredLeft);

    // Box 1: GitHub CI/CD Actions Auto-Compiler
    auto box1 = juce::Rectangle<float>(card.getX() + 24, card.getY() + 75, card.getWidth() - 48, 120);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(box1, 6.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(box1, 6.0f, 1.0f);

    g.setColour(juce::Colour(0xff22d3ee));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("1. GITHUB ACTIONS AUTOMATED CLOUD COMPILER", (int)box1.getX() + 14, (int)box1.getY() + 10, 480, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffd4d4d8));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    juce::String ghText = 
        "* Push this repository to GitHub or fork it.\n"
        "* The included .github/workflows/build-vst.yml CI pipeline triggers automatically.\n"
        "* GitHub builds macOS (Universal VST3/AU), Windows (x64 VST3), and Linux (x64 VST3).\n"
        "* Download pre-built installers directly from the GitHub Actions Artifacts tab!";
    g.drawMultiLineText(ghText, (int)box1.getX() + 14, (int)box1.getY() + 34, (int)box1.getWidth() - 28);

    // Box 2: Local CMake Build Command
    auto box2 = juce::Rectangle<float>(card.getX() + 24, card.getY() + 205, card.getWidth() - 48, 140);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(box2, 6.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(box2, 6.0f, 1.0f);

    g.setColour(juce::Colour(0xffeab308));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("2. LOCAL COMMAND LINE COMPILATION", (int)box2.getX() + 14, (int)box2.getY() + 10, 480, 18, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    juce::String localText = 
        "Requirements: CMake 3.22+, C++20 compiler (Xcode / Visual Studio 2022 / GCC).\n\n"
        "  cmake -B build -DCMAKE_BUILD_TYPE=Release\n"
        "  cmake --build build --config Release\n\n"
        "Binaries output to: build/PhotoSynth_artefacts/Release/VST3/PhotoSynth.vst3";
    g.drawMultiLineText(localText, (int)box2.getX() + 14, (int)box2.getY() + 34, (int)box2.getWidth() - 28);
}

void ExportModalComponent::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre(580, 420);
    closeBtn.setBounds(card.getRight() - 100, card.getBottom() - 44, 80, 28);
}

// -----------------------------------------------------------------------------
// 9. Main Plugin Editor
// -----------------------------------------------------------------------------
PhotoSynthAudioEditor::PhotoSynthAudioEditor (PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), pianoKeyboard(p)
{
    setLookAndFeel (&customLookAndFeel);
    setWantsKeyboardFocus(true);

    // Add Logo & Header Controls
    addAndMakeVisible (logoBox);
    logoBox.onClick = [this] {
        exportModal = std::make_unique<ExportModalComponent>(audioProcessor, [this] {
            exportModal.reset();
            repaint();
        });
        addAndMakeVisible (*exportModal);
        exportModal->setBounds (getLocalBounds());
    };

    addAndMakeVisible (autoSweepBtn);
    addAndMakeVisible (slotABox);
    addAndMakeVisible (morphSlider);
    addAndMakeVisible (slotBBox);
    addAndMakeVisible (bpmBtn);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (savePresetBtn);
    addAndMakeVisible (effectsBtn);
    addAndMakeVisible (deepDiveBtn);

    // BPM Button & interactive popup tempo changer
    bpmBtn.setTooltip ("Tempo BPM - Click to change tempo (20 - 300 BPM)");
    bpmBtn.setButtonText (juce::String(juce::roundToInt(audioProcessor.getBpm())) + " BPM");
    bpmBtn.onClick = [this] {
        juce::PopupMenu menu;
        menu.addSectionHeader ("TEMPO BPM");
        int currentBpmInt = juce::roundToInt(audioProcessor.getBpm());
        int commonBpms[] = { 60, 75, 80, 90, 100, 110, 120, 128, 130, 140, 150, 160, 175 };
        for (int b : commonBpms) {
            menu.addItem (b, juce::String(b) + " BPM", true, (b == currentBpmInt));
        }
        menu.addSeparator();
        menu.addItem (999, "+5 BPM", true, false);
        menu.addItem (998, "-5 BPM", true, false);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bpmBtn), [this](int result) {
            if (result >= 20 && result <= 300) {
                audioProcessor.setBpm ((float)result);
            } else if (result == 999) {
                audioProcessor.setBpm (juce::jmin(300.0f, audioProcessor.getBpm() + 5.0f));
            } else if (result == 998) {
                audioProcessor.setBpm (juce::jmax(20.0f, audioProcessor.getBpm() - 5.0f));
            }
            bpmBtn.setButtonText (juce::String(juce::roundToInt(audioProcessor.getBpm())) + " BPM");
        });
    };

    // Auto Sweep Morph Toggle Button
    autoSweepBtn.onClick = [this] {
        isAutoSweeping = !isAutoSweeping;
        autoSweepBtn.setButtonText(isAutoSweeping ? "||" : ">");
    };

    // Slot A Dropdown items
    slotABox.addItem ("1. Synthwave Sunset", 1);
    slotABox.addItem ("2. Obsidian Crypt", 2);
    slotABox.addItem ("3. Refractive Prism", 3);
    slotABox.addItem ("4. Neon Aurora", 4);
    slotABox.addItem ("+ Upload Image A...", 5);
    slotABox.setSelectedId (1, juce::dontSendNotification);
    slotABox.onChange = [this] {
        int id = slotABox.getSelectedId();
        if (id >= 1 && id <= 4)
            audioProcessor.loadSlotAPreset(id - 1);
        else if (id == 5)
            openImageFileChooser(true);
        updateEraText();
        repaint();
    };

    // Slot B Dropdown items
    slotBBox.addItem ("1. Synthwave Sunset", 1);
    slotBBox.addItem ("2. Obsidian Crypt", 2);
    slotBBox.addItem ("3. Refractive Prism", 3);
    slotBBox.addItem ("4. Neon Aurora", 4);
    slotBBox.addItem ("+ Upload Image B...", 5);
    slotBBox.setSelectedId (2, juce::dontSendNotification);
    slotBBox.onChange = [this] {
        int id = slotBBox.getSelectedId();
        if (id >= 1 && id <= 4)
            audioProcessor.loadSlotBPreset(id - 1);
        else if (id == 5)
            openImageFileChooser(false);
        updateEraText();
        repaint();
    };

    // Morph Slider
    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morphSlider.setRange (0.0, 1.0, 0.001);
    morphAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "morph", morphSlider);
    morphSlider.onValueChange = [this] {
        audioProcessor.updateParametersFromAnalysis();
        updateEraText();
        repaint();
    };

    // Full Preset Dropdown
    refreshPresetDropdown();
    presetBox.onChange = [this] {
        int id = presetBox.getSelectedId();
        if (id >= 1 && id <= 4) {
            audioProcessor.loadSlotAPreset(id - 1);
            audioProcessor.loadSlotBPreset((id) % 4);
            if (auto* param = audioProcessor.apvts.getParameter("morph"))
                param->setValueNotifyingHost(0.0f);
            morphSlider.setValue(0.0, juce::dontSendNotification);
            slotABox.setSelectedId(id, juce::dontSendNotification);
            slotBBox.setSelectedId(((id) % 4) + 1, juce::dontSendNotification);
            audioProcessor.updateParametersFromAnalysis();
        } else if (id >= 100) {
            int userIndex = id - 100;
            audioProcessor.loadUserPresetByIndex(userIndex);
            float currentMorph = *audioProcessor.apvts.getRawParameterValue("morph");
            morphSlider.setValue(currentMorph, juce::dontSendNotification);
        }
        updateEraText();
        repaint();
    };

    // Action buttons
    savePresetBtn.onClick = [this] {
        savePresetModal = std::make_unique<SavePresetModalComponent>(
            audioProcessor,
            [this] { savePresetModal.reset(); repaint(); },
            [this](const juce::String& savedName) {
                refreshPresetDropdown();
                auto names = audioProcessor.getUserPresetNames();
                int idx = names.indexOf(savedName);
                if (idx >= 0)
                    presetBox.setSelectedId(100 + idx, juce::dontSendNotification);
                repaint();
            }
        );
        addAndMakeVisible (*savePresetModal);
        savePresetModal->setBounds (getLocalBounds());
    };

    effectsBtn.onClick = [this] {
        effectsModal = std::make_unique<EffectsModalComponent>(
            audioProcessor,
            customLookAndFeel,
            [this] { effectsModal.reset(); repaint(); }
        );
        addAndMakeVisible (*effectsModal);
        effectsModal->setBounds (getLocalBounds());
    };

    deepDiveBtn.onClick = [this] {
        deepDiveModal = std::make_unique<DeepDiveModalComponent>(
            audioProcessor,
            [this] { deepDiveModal.reset(); repaint(); }
        );
        addAndMakeVisible (*deepDiveModal);
        deepDiveModal->setBounds (getLocalBounds());
    };

    // Left Column: Radial Gauges
    addAndMakeVisible (colorGauge);
    addAndMakeVisible (brightnessGauge);
    addAndMakeVisible (saturationGauge);
    addAndMakeVisible (complexityGauge);

    // Center Column: Oscilloscope & Keyboard
    addAndMakeVisible (oscilloscope);
    addAndMakeVisible (pianoKeyboard);

    // Right Column: Knobs
    auto setupRotaryKnob = [this](juce::Slider& knob, const juce::String& name) {
        knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setName (name);
        addAndMakeVisible (knob);
    };

    setupRotaryKnob (cutoffKnob, "Cutoff");
    setupRotaryKnob (resKnob, "Resonance");
    setupRotaryKnob (lfoRateKnob, "LFO Rate");
    setupRotaryKnob (lfoDepthKnob, "LFO Depth");
    setupRotaryKnob (volKnob, "Master Volume");
    setupRotaryKnob (attackKnob, "Attack");
    setupRotaryKnob (decayKnob, "Decay");
    setupRotaryKnob (sustainKnob, "Sustain");
    setupRotaryKnob (releaseKnob, "Release");

    addAndMakeVisible (limiterButton);

    // Bind APVTS attachments
    cutoffAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff", cutoffKnob);
    resAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "resonance", resKnob);
    lfoRateAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "lfoRate", lfoRateKnob);
    lfoDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "lfoDepth", lfoDepthKnob);
    volAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "volume", volKnob);
    attackAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "attack", attackKnob);
    decayAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "decay", decayKnob);
    sustainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "sustain", sustainKnob);
    releaseAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "release", releaseKnob);
    limiterAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "limiter", limiterButton);

    updateEraText();

    // Responsive native dimensions matching 1:1 design
    setSize (1024, 720);
    setResizable (true, true);
    setResizeLimits (900, 600, 1440, 960);

    startTimerHz (30); // 30 FPS UI refresh & oscilloscope sync
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    setLookAndFeel (nullptr);
}

void PhotoSynthAudioEditor::refreshPresetDropdown()
{
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem ("Preset: Synthwave Sunset", 1);
    presetBox.addItem ("Preset: Obsidian Crypt", 2);
    presetBox.addItem ("Preset: Refractive Prism", 3);
    presetBox.addItem ("Preset: Neon Aurora", 4);

    auto userPresets = audioProcessor.getUserPresetNames();
    for (int i = 0; i < userPresets.size(); ++i) {
        presetBox.addItem ("* " + userPresets[i], 100 + i);
    }
    presetBox.setSelectedId(1, juce::dontSendNotification);
}

void PhotoSynthAudioEditor::updateEraText()
{
    auto analysis = audioProcessor.getLatestAnalysis();
    currentEraText = "ERA: " + analysis.temporalEra.eraLabel;
}

void PhotoSynthAudioEditor::openImageFileChooser(bool isSlotA)
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an Image to Synthesize",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.jpg;*.jpeg;*.png;*.webp"
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this, isSlotA](const juce::FileChooser& fc) {
        auto file = fc.getResult();
        if (file.existsAsFile()) {
            auto loaded = juce::ImageFileFormat::loadFrom(file);
            if (loaded.isValid()) {
                if (isSlotA)
                    audioProcessor.loadSlotAImage(loaded, file.getFileNameWithoutExtension());
                else
                    audioProcessor.loadSlotBImage(loaded, file.getFileNameWithoutExtension());

                updateEraText();
                repaint();
            }
        }
    });
}

bool PhotoSynthAudioEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) {
        if (f.endsWithIgnoreCase(".jpg") || f.endsWithIgnoreCase(".jpeg") || f.endsWithIgnoreCase(".png"))
            return true;
    }
    return false;
}

void PhotoSynthAudioEditor::filesDropped (const juce::StringArray& files, int x, int)
{
    for (const auto& f : files) {
        juce::File file (f);
        if (file.existsAsFile()) {
            auto loaded = juce::ImageFileFormat::loadFrom(file);
            if (loaded.isValid()) {
                // If dropped on left side of image, load to Slot A; else Slot B
                if (x < imageBounds.getCentreX())
                    audioProcessor.loadSlotAImage(loaded, file.getFileNameWithoutExtension());
                else
                    audioProcessor.loadSlotBImage(loaded, file.getFileNameWithoutExtension());

                updateEraText();
                repaint();
                break;
            }
        }
    }
}

void PhotoSynthAudioEditor::mouseUp(const juce::MouseEvent& e)
{
    // Click on image box opens image file chooser
    if (imageBounds.contains(e.getPosition())) {
        bool isLeftHalf = (e.getPosition().getX() < imageBounds.getCentreX());
        openImageFileChooser(isLeftHalf);
    }
}

void PhotoSynthAudioEditor::timerCallback()
{
    // 1. Auto Sweep morph slider if active
    if (isAutoSweeping) {
        float currentMorph = (float)morphSlider.getValue();
        currentMorph += sweepDirection * 0.008f;
        if (currentMorph >= 1.0f) {
            currentMorph = 1.0f;
            sweepDirection = -1.0f;
        } else if (currentMorph <= 0.0f) {
            currentMorph = 0.0f;
            sweepDirection = 1.0f;
        }
        morphSlider.setValue(currentMorph, juce::sendNotificationSync);
    }

    // 2. Update Gauges from processor
    auto analysis = audioProcessor.getLatestAnalysis();
    colorGauge.setValue (analysis.timbreDna);
    brightnessGauge.setValue (analysis.brightness);
    saturationGauge.setValue (analysis.saturation);
    complexityGauge.setValue (analysis.complexity);

    // 3. Update Oscilloscope
    float scopeData[256];
    audioProcessor.getScopeData (scopeData, 256);
    oscilloscope.pushData (scopeData, 256);

    bpmBtn.setButtonText (juce::String(juce::roundToInt(audioProcessor.getBpm())) + " BPM");
    updateEraText();
}

void PhotoSynthAudioEditor::paint (juce::Graphics& g)
{
    // Main Dark Chassis
    g.fillAll (juce::Colour(0xff09090b));

    // Outer Bezel & Border
    g.setColour (juce::Colour(0xff27272a));
    g.drawRect (getLocalBounds().toFloat(), 1.5f);

    // Top Titlebar Embossed Brand Text
    g.setColour (juce::Colour(0xff141417));
    g.fillRect (80, 0, getWidth() - 80, 32);
    g.setColour (juce::Colour(0xff27272a));
    g.drawHorizontalLine (32, 80, (float)getWidth());

    g.setColour (juce::Colour(0xfff4f4f5));
    g.setFont (juce::Font(12.0f, juce::Font::bold));
    g.drawText ("PAINT A SYNTH", 80, 0, getWidth() - 80, 32, juce::Justification::centred);

    // Subheader background bar
    g.setColour (juce::Colour(0xff121215));
    g.fillRect (80, 32, getWidth() - 80, 42);
    g.setColour (juce::Colour(0xff27272a));
    g.drawHorizontalLine (74, 0, (float)getWidth());

    // Morph label
    g.setColour (juce::Colour(0xff22d3ee));
    g.setFont (juce::Font(10.0f, juce::Font::bold));
    g.drawText ("A / B MORPH", morphSlider.getX(), morphSlider.getY() - 12, morphSlider.getWidth(), 12, juce::Justification::centred);

    // Center Column: Render Blended Image
    g.setColour (juce::Colour(0xff121215));
    g.fillRect (imageBounds);
    g.setColour (juce::Colour(0xff27272a));
    g.drawRect (imageBounds.toFloat(), 1.5f);

    auto blended = audioProcessor.getBlendedImage();
    if (blended.isValid()) {
        g.drawImage (blended, imageBounds.toFloat(), juce::RectanglePlacement::centred);
    }

    // Change image overlay buttons text
    g.setColour (juce::Colour(0xbb000000));
    g.fillRoundedRectangle (imageBounds.getX() + 8, imageBounds.getBottom() - 28, 120, 20, 3.0f);
    g.fillRoundedRectangle (imageBounds.getRight() - 128, imageBounds.getBottom() - 28, 120, 20, 3.0f);

    g.setColour (juce::Colour(0xff22d3ee));
    g.setFont (juce::Font(9.0f, juce::Font::bold));
    g.drawText ("CHANGE IMAGE A", imageBounds.getX() + 8, imageBounds.getBottom() - 28, 120, 20, juce::Justification::centred);
    g.drawText ("CHANGE IMAGE B", imageBounds.getRight() - 128, imageBounds.getBottom() - 28, 120, 20, juce::Justification::centred);

    // ERA Badge Pill
    int eraPillW = 280;
    int eraPillH = 22;
    int eraPillX = imageBounds.getCentreX() - eraPillW / 2;
    int eraPillY = imageBounds.getBottom() + 6;

    g.setColour (juce::Colour(0xff141417));
    g.fillRoundedRectangle ((float)eraPillX, (float)eraPillY, (float)eraPillW, (float)eraPillH, 11.0f);
    g.setColour (juce::Colour(0xff22d3ee));
    g.drawRoundedRectangle ((float)eraPillX, (float)eraPillY, (float)eraPillW, (float)eraPillH, 11.0f, 1.0f);

    g.setColour (juce::Colour(0xfff4f4f5));
    g.setFont (juce::Font(10.0f, juce::Font::bold));
    g.drawText (currentEraText, eraPillX, eraPillY, eraPillW, eraPillH, juce::Justification::centred);

    // Left Column Panel
    int leftW = (int)(getWidth() * 0.22f);
    auto leftPanel = juce::Rectangle<int>(10, 84, leftW, getHeight() - 94);
    g.setColour (juce::Colour(0xff121215));
    g.fillRoundedRectangle (leftPanel.toFloat(), 6.0f);
    g.setColour (juce::Colour(0xff27272a));
    g.drawRoundedRectangle (leftPanel.toFloat(), 6.0f, 1.2f);

    // Left Panel Header
    g.setColour (juce::Colour(0xfff4f4f5));
    g.setFont (juce::Font(11.0f, juce::Font::bold));
    g.drawText ("TIMBRE DNA", leftPanel.getX() + 14, leftPanel.getY() + 10, 100, 20, juce::Justification::centredLeft);

    // Right Column Panel
    int rightW = (int)(getWidth() * 0.32f);
    auto rightPanel = juce::Rectangle<int>(getWidth() - rightW - 10, 84, rightW, getHeight() - 94);
    g.setColour (juce::Colour(0xff121215));
    g.fillRoundedRectangle (rightPanel.toFloat(), 6.0f);
    g.setColour (juce::Colour(0xff27272a));
    g.drawRoundedRectangle (rightPanel.toFloat(), 6.0f, 1.2f);

    // Right Subpanel Headers
    g.setColour (juce::Colour(0xfff4f4f5));
    g.setFont (juce::Font(11.0f, juce::Font::bold));
    g.drawText ("MANUAL OVERRIDE", rightPanel.getX() + 14, rightPanel.getY() + 10, 160, 20, juce::Justification::centredLeft);

    int midDividerY = rightPanel.getY() + (rightPanel.getHeight() * 0.44f);
    g.setColour (juce::Colour(0xff27272a));
    g.drawHorizontalLine (midDividerY, (float)rightPanel.getX() + 6.0f, (float)rightPanel.getRight() - 6.0f);

    g.setColour (juce::Colour(0xfff4f4f5));
    g.drawText ("DYNAMICS / OUTPUT", rightPanel.getX() + 14, midDividerY + 8, 160, 20, juce::Justification::centredLeft);

    // Knob Labels
    auto drawKnobLabel = [&](juce::Slider& s, const juce::String& lbl, const juce::String& val) {
        g.setColour (juce::Colour(0xffa1a1aa));
        g.setFont (juce::Font(9.0f, juce::Font::bold));
        g.drawText (lbl, s.getX() - 6, s.getY() - 14, s.getWidth() + 12, 12, juce::Justification::centred);
        g.setColour (juce::Colour(0xff22d3ee));
        g.setFont (juce::Font(9.0f, juce::Font::plain));
        g.drawText (val, s.getX() - 6, s.getBottom() + 1, s.getWidth() + 12, 12, juce::Justification::centred);
    };

    drawKnobLabel (cutoffKnob, "CUTOFF", juce::String(juce::roundToInt((float)cutoffKnob.getValue())) + " Hz");
    drawKnobLabel (resKnob, "RESONANCE", juce::String((float)resKnob.getValue(), 2) + " Q");
    drawKnobLabel (lfoRateKnob, "LFO RATE", juce::String((float)lfoRateKnob.getValue(), 1) + " Hz");
    drawKnobLabel (lfoDepthKnob, "LFO DEPTH", juce::String((float)lfoDepthKnob.getValue(), 2));

    drawKnobLabel (volKnob, "MASTER VOL", juce::String((float)volKnob.getValue(), 1) + " dB");
    drawKnobLabel (attackKnob, "ATTACK", juce::String((float)attackKnob.getValue(), 3) + " s");
    drawKnobLabel (decayKnob, "DECAY", juce::String((float)decayKnob.getValue(), 2) + " s");
    drawKnobLabel (sustainKnob, "SUSTAIN", juce::String(juce::roundToInt((float)sustainKnob.getValue() * 100.0f)) + "%");
    drawKnobLabel (releaseKnob, "RELEASE", juce::String((float)releaseKnob.getValue(), 2) + " s");
}

void PhotoSynthAudioEditor::resized()
{
    // Resize modal overlays if active
    if (effectsModal) effectsModal->setBounds (getLocalBounds());
    if (savePresetModal) savePresetModal->setBounds (getLocalBounds());
    if (deepDiveModal) deepDiveModal->setBounds (getLocalBounds());
    if (exportModal) exportModal->setBounds (getLocalBounds());

    // 1. Logo Box on top left
    logoBox.setBounds (0, 0, 80, 74);

    // 2. Sub-Header Controls
    int subY = 38;
    autoSweepBtn.setBounds (92, subY, 28, 26);
    slotABox.setBounds (126, subY, 130, 26);
    morphSlider.setBounds (264, subY, 120, 26);
    slotBBox.setBounds (392, subY, 130, 26);

    bpmBtn.setBounds (getWidth() - 295, subY, 78, 26);
    presetBox.setBounds (getWidth() - 210, subY, 140, 26);
    savePresetBtn.setBounds (getWidth() - 65, subY, 55, 26);

    // 3. Left Column: TIMBRE DNA & Radial Gauges
    int leftW = (int)(getWidth() * 0.22f);
    auto leftPanel = juce::Rectangle<int>(10, 84, leftW, getHeight() - 94);
    effectsBtn.setBounds (leftPanel.getRight() - 75, leftPanel.getY() + 8, 65, 22);

    int gaugeSize = juce::jlimit(60, 95, (leftPanel.getHeight() - 90) / 4);
    int gaugeX = leftPanel.getCentreX() - gaugeSize / 2;
    int startGY = leftPanel.getY() + 38;
    int gapY = (leftPanel.getHeight() - 85 - gaugeSize * 4) / 3;

    colorGauge.setBounds (gaugeX, startGY, gaugeSize, gaugeSize);
    brightnessGauge.setBounds (gaugeX, startGY + (gaugeSize + gapY), gaugeSize, gaugeSize);
    saturationGauge.setBounds (gaugeX, startGY + (gaugeSize + gapY) * 2, gaugeSize, gaugeSize);
    complexityGauge.setBounds (gaugeX, startGY + (gaugeSize + gapY) * 3, gaugeSize, gaugeSize);

    deepDiveBtn.setBounds (leftPanel.getX() + 10, leftPanel.getBottom() - 30, leftPanel.getWidth() - 20, 22);

    // 4. Center Column: Image, Oscilloscope & Keyboard
    int rightW = (int)(getWidth() * 0.32f);
    int centerLeft = leftPanel.getRight() + 10;
    int centerW = getWidth() - rightW - centerLeft - 20;

    int imgH = juce::jlimit(180, 290, (int)(getHeight() * 0.38f));
    imageBounds = juce::Rectangle<int>(centerLeft, 84, centerW, imgH);

    int scopeY = imageBounds.getBottom() + 36;
    int scopeH = juce::jlimit(45, 75, (int)(getHeight() * 0.10f));
    oscilloscope.setBounds (centerLeft, scopeY, centerW, scopeH);

    int pianoY = scopeY + scopeH + 10;
    int pianoH = juce::jmax(65, getHeight() - pianoY - 14);
    pianoKeyboard.setBounds (centerLeft, pianoY, centerW, pianoH);

    // 5. Right Column: Manual Override Knobs
    auto rightPanel = juce::Rectangle<int>(getWidth() - rightW - 10, 84, rightW, getHeight() - 94);

    int knobD = juce::jlimit(44, 56, (rightPanel.getWidth() - 60) / 2);
    int rCol1 = rightPanel.getX() + 24;
    int rCol2 = rightPanel.getX() + rightPanel.getWidth() - knobD - 24;

    // Row 1 (Cutoff, Resonance)
    int rRow1Y = rightPanel.getY() + 42;
    cutoffKnob.setBounds (rCol1, rRow1Y, knobD, knobD);
    resKnob.setBounds (rCol2, rRow1Y, knobD, knobD);

    // Row 2 (LFO Rate, LFO Depth)
    int rRow2Y = rRow1Y + knobD + 26;
    lfoRateKnob.setBounds (rCol1, rRow2Y, knobD, knobD);
    lfoDepthKnob.setBounds (rCol2, rRow2Y, knobD, knobD);

    // Section 2: Dynamics
    int midDividerY = rightPanel.getY() + (rightPanel.getHeight() * 0.44f);
    int rRow3Y = midDividerY + 40;
    volKnob.setBounds (rCol1, rRow3Y, knobD, knobD);
    attackKnob.setBounds (rCol2, rRow3Y, knobD, knobD);

    int rRow4Y = rRow3Y + knobD + 26;
    decayKnob.setBounds (rCol1, rRow4Y, knobD, knobD);
    sustainKnob.setBounds (rCol2, rRow4Y, knobD, knobD);

    int rRow5Y = rRow4Y + knobD + 26;
    releaseKnob.setBounds (rCol1, rRow5Y, knobD, knobD);
    limiterButton.setBounds (rCol2, rRow5Y + 12, knobD + 10, 24);
}

bool PhotoSynthAudioEditor::keyPressed(const juce::KeyPress& key)
{
    return pianoKeyboard.keyPressed(key);
}

bool PhotoSynthAudioEditor::keyStateChanged(bool isKeyDown)
{
    return pianoKeyboard.keyStateChanged(isKeyDown);
}

