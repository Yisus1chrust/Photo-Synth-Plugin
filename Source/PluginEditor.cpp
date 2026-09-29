/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Photo Synth
    Hybrid Audio Plugin Architecture (EVERSYNTH Architecture)
    100% 1:1 Accurate Web Aesthetic with Real-Time Native C++ DSP Engine
  ==============================================================================
*/

#include "PluginEditor.h"
#include "LogoBinaryData.h"
#include <cstddef>
#include <cstring>

#if JUCE_WEB_BROWSER

// ==============================================================================
// 1. Hybrid Web Editor Implementation (EVERSYNTH Architecture)
// ==============================================================================

PhotoSynthAudioEditor::PhotoSynthAudioEditor(PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set standard 16:10 / 4:3 high-res canvas size matching the React UI
    setSize(1160, 840);
    setResizable(true, true);
    setResizeLimits(880, 640, 2560, 1600);

    // Build JUCE 8 WebBrowserComponent Options
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withResourceProvider([this](const juce::String& path) {
            return getResource(path);
        })
        // 1. setParameter native function
        .withNativeFunction("setParameter", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 2) {
                juce::String paramId = args[0].toString();
                float value = static_cast<float>(args[1].operator double());

                // Remap frontend parameter names to APVTS IDs if needed
                if (paramId == "cutoffOffset") paramId = "cutoff";
                else if (paramId == "attackTime") paramId = "attack";
                else if (paramId == "decayTime") paramId = "decay";
                else if (paramId == "sustainLevel") paramId = "sustain";
                else if (paramId == "releaseTime") paramId = "release";
                else if (paramId == "masterVolume") paramId = "volume";
                else if (paramId == "limiterEnabled") paramId = "limiter";

                if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
                    param->setValueNotifyingHost(param->convertTo0to1(value));
                }
            }
            completion(juce::var());
        })
        // 1b. syncPatch native function (syncs all patch parameters at once)
        .withNativeFunction("syncPatch", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1 && args[0].isObject()) {
                auto* obj = args[0].getDynamicObject();
                if (obj != nullptr) {
                    for (const auto& prop : obj->getProperties()) {
                        juce::String paramId = prop.name.toString();
                        float value = static_cast<float>(prop.value.operator double());

                        if (paramId == "cutoffOffset") paramId = "cutoff";
                        else if (paramId == "attackTime") paramId = "attack";
                        else if (paramId == "decayTime") paramId = "decay";
                        else if (paramId == "sustainLevel") paramId = "sustain";
                        else if (paramId == "releaseTime") paramId = "release";
                        else if (paramId == "masterVolume") paramId = "volume";
                        else if (paramId == "limiterEnabled") paramId = "limiter";

                        if (auto* param = audioProcessor.apvts.getParameter(paramId)) {
                            param->setValueNotifyingHost(param->convertTo0to1(value));
                        }
                    }
                }
            }
            completion(juce::var());
        })
        // 1c. openFileChooser native function for image loading in VST/AU
        .withNativeFunction("openFileChooser", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            juce::String slot = args.size() >= 1 ? args[0].toString() : "A";

            fileChooser = std::make_unique<juce::FileChooser>(
                "Select Image for Slot " + slot,
                juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
                "*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif");

            auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

            fileChooser->launchAsync(folderFlags, [this, slot, completion](const juce::FileChooser& fc) {
                auto resultFile = fc.getResult();
                if (resultFile.existsAsFile()) {
                    juce::MemoryBlock mb;
                    if (resultFile.loadFileAsData(mb)) {
                        juce::String b64 = juce::Base64::toBase64(mb.getData(), mb.getSize());
                        juce::String ext = resultFile.getFileExtension().toLowerCase();
                        juce::String mime = "image/png";
                        if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
                        else if (ext == ".webp") mime = "image/webp";
                        else if (ext == ".gif") mime = "image/gif";

                        auto img = juce::ImageFileFormat::loadFrom(resultFile);
                        juce::String dataUrl;
                        if (img.isValid()) {
                            // Downscale thumbnail for WebView to max 512px so IPC transfer is light (<50KB)
                            juce::Image thumb = img;
                            const int maxDim = 512;
                            if (thumb.getWidth() > maxDim || thumb.getHeight() > maxDim) {
                                int tw = thumb.getWidth();
                                int th = thumb.getHeight();
                                if (tw > th) {
                                    th = std::max(1, (th * maxDim) / tw);
                                    tw = maxDim;
                                } else {
                                    tw = std::max(1, (tw * maxDim) / th);
                                    th = maxDim;
                                }
                                thumb = thumb.rescaled(tw, th, juce::Graphics::mediumResamplingQuality);
                            }
                            juce::MemoryOutputStream mos;
                            juce::JPEGImageFormat jpeg;
                            jpeg.writeImageToStream(thumb, mos);
                            juce::String b64Thumb = juce::Base64::toBase64(mos.getData(), mos.getDataSize());
                            dataUrl = "data:image/jpeg;base64," + b64Thumb;

                            if (slot == "B") {
                                audioProcessor.loadSlotBImage(img, resultFile.getFileNameWithoutExtension());
                            } else {
                                audioProcessor.loadSlotAImage(img, resultFile.getFileNameWithoutExtension());
                            }
                        } else {
                            dataUrl = "data:" + mime + ";base64," + b64;
                        }

                        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
                        resObj->setProperty("slot", slot);
                        resObj->setProperty("fileName", resultFile.getFileName());
                        resObj->setProperty("dataUrl", dataUrl);
                        completion(juce::var(resObj.get()));
                        return;
                    }
                }
                completion(juce::var());
            });
        })
        // 2. noteOn native function
        .withNativeFunction("noteOn", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                float vel = args.size() > 1 ? static_cast<float>(args[1].operator double()) : 0.85f;
                audioProcessor.noteOn(note, vel);
            }
            completion(juce::var());
        })
        // 3. noteOff native function
        .withNativeFunction("noteOff", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                audioProcessor.noteOff(note);
            }
            completion(juce::var());
        })
        // 4. setBpm native function
        .withNativeFunction("setBpm", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                float bpm = static_cast<float>(args[0].operator double());
                audioProcessor.setBpm(bpm);
            }
            completion(juce::var());
        })
        // 5. savePreset native function
        .withNativeFunction("savePreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                juce::String name = args[0].toString();
                juce::String desc = args.size() > 1 ? args[1].toString() : "";
                audioProcessor.saveCurrentPreset(name, desc);
            }
            completion(juce::var());
        })
        // 6. loadPreset native function
        .withNativeFunction("loadPreset", [this](const juce::Array<juce::var>& args, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            if (args.size() >= 1) {
                juce::String name = args[0].toString();
                audioProcessor.loadUserPreset(name);
            }
            completion(juce::var());
        })
        // 7. getWaveformData native function
        .withNativeFunction("getWaveformData", [this](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            float scopeBuffer[256];
            audioProcessor.getScopeData(scopeBuffer, 256);
            juce::Array<juce::var> arr;
            for (int i = 0; i < 256; ++i) {
                arr.add((double)scopeBuffer[i]);
            }
            completion(arr);
        })
        // 8. getInitialState native function
        .withNativeFunction("getInitialState", [this](const juce::Array<juce::var>&, juce::WebBrowserComponent::NativeFunctionCompletion completion) {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("bpm", audioProcessor.getBpm());
            obj->setProperty("isNativeHost", true);

            auto* paramsObj = new juce::DynamicObject();
            for (auto* param : audioProcessor.getParameters()) {
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(param)) {
                    paramsObj->setProperty(withId->paramID, param->getValue());
                }
            }
            obj->setProperty("parameters", juce::var(paramsObj));
            completion(juce::var(obj));
        });

    // Create and attach WebView
    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // Navigate to offline local resource provider root
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    // Start 30 FPS timer for real-time oscilloscope & automation streaming (gentle on WebView IPC)
    startTimerHz(30);
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    stopTimer();
    webView = nullptr;
}

void PhotoSynthAudioEditor::paint(juce::Graphics& g)
{
    // Clean modern chassis background before webview initializes
    g.fillAll(juce::Colour(0xff09090b));
}

void PhotoSynthAudioEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds(getLocalBounds());
}

juce::File PhotoSynthAudioEditor::getUiDirectory() const
{
    // 1. Check inside macOS app/plugin bundle (Contents/Resources/ui/)
    auto bundleDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto macResources = bundleDir.getParentDirectory().getChildFile("Resources").getChildFile("ui");
    if (macResources.isDirectory()) return macResources;

    // 2. Check sibling "ui" or "dist" directory (Windows & Linux VST3 / Standalone)
    if (bundleDir.getChildFile("ui").isDirectory()) return bundleDir.getChildFile("ui");
    if (bundleDir.getChildFile("dist").isDirectory()) return bundleDir.getChildFile("dist");

    // 3. Check User AppData directory
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("ModularSoundTechnologies")
                        .getChildFile("PhotoSynth")
                        .getChildFile("ui");
    if (appData.isDirectory()) return appData;

    // 4. Current working directory fallback
    auto cwdDist = juce::File::getCurrentWorkingDirectory().getChildFile("dist");
    if (cwdDist.isDirectory()) return cwdDist;

    return {};
}

std::optional<juce::WebBrowserComponent::Resource> PhotoSynthAudioEditor::getResource(const juce::String& url)
{
    auto uiDir = getUiDirectory();
    juce::String relativePath = url;
    if (relativePath.startsWith("/"))
        relativePath = relativePath.substring(1);
    if (relativePath.isEmpty())
        relativePath = "index.html";

    auto file = uiDir.getChildFile(relativePath);
    if (!file.existsAsFile())
    {
        // Fallback to index.html for client-side routing
        file = uiDir.getChildFile("index.html");
    }

    if (file.existsAsFile())
    {
        juce::MemoryBlock mem;
        if (file.loadFileAsData(mem))
        {
            std::vector<std::byte> bytes(mem.getSize());
            std::memcpy(bytes.data(), mem.getData(), mem.getSize());

            juce::String mime = "application/octet-stream";
            if (relativePath.endsWithIgnoreCase(".html") || relativePath.endsWithIgnoreCase(".htm"))
                mime = "text/html; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".js") || relativePath.endsWithIgnoreCase(".mjs"))
                mime = "application/javascript; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".css"))
                mime = "text/css; charset=utf-8";
            else if (relativePath.endsWithIgnoreCase(".png"))
                mime = "image/png";
            else if (relativePath.endsWithIgnoreCase(".jpg") || relativePath.endsWithIgnoreCase(".jpeg"))
                mime = "image/jpeg";
            else if (relativePath.endsWithIgnoreCase(".svg"))
                mime = "image/svg+xml";
            else if (relativePath.endsWithIgnoreCase(".json"))
                mime = "application/json";
            else if (relativePath.endsWithIgnoreCase(".woff2"))
                mime = "font/woff2";

            return juce::WebBrowserComponent::Resource { std::move(bytes), mime };
        }
    }

    // Emergency embedded fallback if dist folder is absent
    static const char* fallbackHtml =
        "<!DOCTYPE html><html><body style='background:#09090b;color:#e4e4e7;font-family:sans-serif;"
        "display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;margin:0;'>"
        "<h2 style='color:#f59e0b;letter-spacing:2px;'>PHOTO SYNTH HYBRID ENGINE</h2>"
        "<p style='color:#71717a;font-size:12px;'>Building web bundle into plugin resources...</p>"
        "</body></html>";

    std::vector<std::byte> fallbackBytes;
    size_t len = strlen(fallbackHtml);
    fallbackBytes.resize(len);
    std::memcpy(fallbackBytes.data(), fallbackHtml, len);

    return juce::WebBrowserComponent::Resource { std::move(fallbackBytes), "text/html; charset=utf-8" };
}

void PhotoSynthAudioEditor::timerCallback()
{
    if (webView == nullptr) return;

    // 1. Fetch real-time waveform buffer
    float scopeBuffer[256];
    audioProcessor.getScopeData(scopeBuffer, 256);

    juce::String js;
    js.preallocateBytes(2048);
    js << "if(window.__PhotoSynthWebview&&window.__PhotoSynthWebview.onWaveformUpdate){"
       << "window.__PhotoSynthWebview.onWaveformUpdate([";
    for (int i = 0; i < 256; ++i) {
        js << juce::String(scopeBuffer[i], 3);
        if (i < 255) js << ",";
    }
    js << "]);}";
    webView->evaluateJavascript(js);

    // 2. Sync DAW host tempo changes
    float currentBpm = audioProcessor.getBpm();
    if (std::abs(currentBpm - lastReportedBpm) > 0.05f) {
        lastReportedBpm = currentBpm;
        juce::String bpmJs = "if(window.__PhotoSynthWebview&&window.__PhotoSynthWebview.onBpmUpdate){"
                             "window.__PhotoSynthWebview.onBpmUpdate(" + juce::String(currentBpm, 1) + ");}";
        webView->evaluateJavascript(bpmJs);
    }
}

#else

// ==============================================================================
// 2. Fallback Native Vector Editor (when compiled with JUCE_WEB_BROWSER=0)
// ==============================================================================

RadialGaugeComponent::RadialGaugeComponent(const juce::String& labelText, juce::Colour ringColor)
    : label(labelText), color(ringColor) {}

void RadialGaugeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    auto center = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.48f;

    g.setColour(juce::Colour(0xff121215));
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

    float arcRadius = radius - 5.0f;
    float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    float endAngle = juce::MathConstants<float>::pi * 0.75f;

    juce::Path trackPath;
    trackPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff1f242e));
    g.strokePath(trackPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    float currentAngle = startAngle + juce::jlimit(0.0f, 1.0f, targetValue) * (endAngle - startAngle);
    juce::Path filledPath;
    filledPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, startAngle, currentAngle, true);
    g.setColour(color);
    g.strokePath(filledPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void OscilloscopeComponent::pushData(const float* samples, int numSamples)
{
    if (numSamples <= 0) return;
    int copyCount = juce::jmin(numSamples, (int)waveformData.size());
    std::memcpy(waveformData.data(), samples, sizeof(float) * copyCount);
    repaint();
}

void OscilloscopeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff06090e));
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xff111927));
    g.drawHorizontalLine((int)(bounds.getHeight() * 0.5f), 0.0f, bounds.getWidth());

    if (waveformData.empty()) return;

    juce::Path path;
    float sliceWidth = bounds.getWidth() / (float)waveformData.size();
    float halfH = bounds.getHeight() * 0.5f;

    for (size_t i = 0; i < waveformData.size(); ++i)
    {
        float x = (float)i * sliceWidth;
        float y = halfH - (waveformData[i] * halfH * 0.85f);
        if (i == 0) path.startNewSubPath(x, y);
        else path.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff38bdf8));
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

VirtualPianoKeyboard::VirtualPianoKeyboard(PhotoSynthAudioProcessor& proc)
    : processor(proc)
{
    setWantsKeyboardFocus(true);
}

VirtualPianoKeyboard::~VirtualPianoKeyboard() = default;

void VirtualPianoKeyboard::resized()
{
    keys.clear();
    int startNote = 48; // C3
    int endNote = 72;   // C5
    int whiteCount = 0;

    for (int n = startNote; n <= endNote; ++n) {
        if (!isBlackKey(n)) whiteCount++;
    }

    float w = (float)getWidth() / (float)whiteCount;
    float h = (float)getHeight();
    float whiteX = 0.0f;

    for (int n = startNote; n <= endNote; ++n) {
        if (!isBlackKey(n)) {
            PianoKeyDef k;
            k.note = n;
            k.isBlack = false;
            k.bounds = juce::Rectangle<float>(whiteX, 0.0f, w, h);
            keys.push_back(k);
            whiteX += w;
        }
    }

    float blackW = w * 0.62f;
    float blackH = h * 0.62f;

    for (int n = startNote; n <= endNote; ++n) {
        if (isBlackKey(n)) {
            for (size_t i = 0; i < keys.size(); ++i) {
                if (keys[i].note == n - 1 && !keys[i].isBlack) {
                    PianoKeyDef k;
                    k.note = n;
                    k.isBlack = true;
                    k.bounds = juce::Rectangle<float>(keys[i].bounds.getRight() - (blackW * 0.5f), 0.0f, blackW, blackH);
                    keys.push_back(k);
                    break;
                }
            }
        }
    }
}

void VirtualPianoKeyboard::paint(juce::Graphics& g)
{
    for (const auto& k : keys) {
        if (!k.isBlack) {
            bool isActive = activeNotes.count(k.note) > 0;
            g.setColour(isActive ? juce::Colour(0xfff59e0b) : juce::Colour(0xfff4f4f5));
            g.fillRect(k.bounds);
            g.setColour(juce::Colour(0xff27272a));
            g.drawRect(k.bounds, 1.0f);
        }
    }

    for (const auto& k : keys) {
        if (k.isBlack) {
            bool isActive = activeNotes.count(k.note) > 0;
            g.setColour(isActive ? juce::Colour(0xffd97706) : juce::Colour(0xff18181b));
            g.fillRect(k.bounds);
            g.setColour(juce::Colour(0xff3f3f46));
            g.drawRect(k.bounds, 1.0f);
        }
    }
}

int VirtualPianoKeyboard::getNoteAtPosition(juce::Point<int> pos)
{
    auto pt = pos.toFloat();
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
        if (it->isBlack && it->bounds.contains(pt)) return it->note;
    }
    for (const auto& k : keys) {
        if (!k.isBlack && k.bounds.contains(pt)) return k.note;
    }
    return -1;
}

bool VirtualPianoKeyboard::isBlackKey(int midiNote)
{
    int semitone = midiNote % 12;
    return (semitone == 1 || semitone == 3 || semitone == 6 || semitone == 8 || semitone == 10);
}

void VirtualPianoKeyboard::mouseDown(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note >= 0) {
        currentMouseNote = note;
        triggerNoteOn(note);
    }
}

void VirtualPianoKeyboard::mouseDrag(const juce::MouseEvent& e)
{
    int note = getNoteAtPosition(e.getPosition());
    if (note >= 0 && note != currentMouseNote) {
        if (currentMouseNote >= 0) triggerNoteOff(currentMouseNote);
        currentMouseNote = note;
        triggerNoteOn(note);
    }
}

void VirtualPianoKeyboard::mouseUp(const juce::MouseEvent&)
{
    if (currentMouseNote >= 0) {
        triggerNoteOff(currentMouseNote);
        currentMouseNote = -1;
    }
}

void VirtualPianoKeyboard::triggerNoteOn(int midiNote, float vel)
{
    activeNotes.insert(midiNote);
    processor.noteOn(midiNote, vel);
    repaint();
}

void VirtualPianoKeyboard::triggerNoteOff(int midiNote)
{
    activeNotes.erase(midiNote);
    processor.noteOff(midiNote);
    repaint();
}

bool VirtualPianoKeyboard::keyPressed(const juce::KeyPress& key)
{
    int note = keyPressToMidi(key);
    if (note >= 0 && activeNotes.count(note) == 0) {
        triggerNoteOn(note);
        return true;
    }
    return false;
}

bool VirtualPianoKeyboard::keyStateChanged(bool isKeyDown)
{
    if (!isKeyDown) {
        std::vector<int> notesToRelease(activeNotes.begin(), activeNotes.end());
        for (int n : notesToRelease) {
            triggerNoteOff(n);
        }
    }
    return true;
}

int VirtualPianoKeyboard::keyPressToMidi(const juce::KeyPress& key)
{
    auto c = std::tolower(key.getTextCharacter());
    switch (c) {
        case 'a': return 60; // C4
        case 'w': return 61; // C#4
        case 's': return 62; // D4
        case 'e': return 63; // D#4
        case 'd': return 64; // E4
        case 'f': return 65; // F4
        case 't': return 66; // F#4
        case 'g': return 67; // G4
        case 'y': return 68; // G#4
        case 'h': return 69; // A4
        case 'u': return 70; // A#4
        case 'j': return 71; // B4
        case 'k': return 72; // C5
        default: return -1;
    }
}

LogoComponent::LogoComponent()
{
    logoImage = LogoBinaryData::getLogoImage();
}

void LogoComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff141417));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(isHovered ? juce::Colour(0xfff59e0b) : juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);

    if (logoImage.isValid()) {
        g.drawImage(logoImage, bounds.reduced(3.0f), juce::RectanglePlacement::centred);
    }
}

void LogoComponent::mouseUp(const juce::MouseEvent&) { if (onClick) onClick(); }
void LogoComponent::mouseEnter(const juce::MouseEvent&) { isHovered = true; repaint(); }
void LogoComponent::mouseExit(const juce::MouseEvent&) { isHovered = false; repaint(); }

BpmEditorComponent::BpmEditorComponent() { updateText(); }
BpmEditorComponent::BpmEditorComponent(PhotoSynthAudioProcessor& proc) : processor(&proc) { updateText(); }
void BpmEditorComponent::setProcessor(PhotoSynthAudioProcessor& proc) { processor = &proc; updateText(); }

void BpmEditorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(isHovered ? juce::Colour(0xff18181b) : juce::Colour(0xff121215));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(isHovered ? juce::Colour(0xff38bdf8) : juce::Colour(0xff27272a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void BpmEditorComponent::resized() { bpmLabel.setBounds(getLocalBounds()); }
void BpmEditorComponent::mouseEnter(const juce::MouseEvent&) { isHovered = true; repaint(); }
void BpmEditorComponent::mouseExit(const juce::MouseEvent&) { isHovered = false; repaint(); }
void BpmEditorComponent::mouseDown(const juce::MouseEvent&) { bpmLabel.showEditor(); }
void BpmEditorComponent::updateText()
{
    if (processor) bpmLabel.setText(juce::String((int)processor->getBpm()) + " BPM", juce::dontSendNotification);
}

PhotoSynthAudioEditor::PhotoSynthAudioEditor(PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), pianoKeyboard(p)
{
    setSize(1080, 800);
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor() = default;

void PhotoSynthAudioEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b));
}

void PhotoSynthAudioEditor::resized() {}
bool PhotoSynthAudioEditor::isInterestedInFileDrag(const juce::StringArray&) { return true; }
void PhotoSynthAudioEditor::filesDropped(const juce::StringArray&, int, int) {}
void PhotoSynthAudioEditor::timerCallback() {}
void PhotoSynthAudioEditor::mouseUp(const juce::MouseEvent&) {}
bool PhotoSynthAudioEditor::keyPressed(const juce::KeyPress&) { return false; }
bool PhotoSynthAudioEditor::keyStateChanged(bool) { return false; }

#endif
