/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Paint a Synth (Photo Synth)
    JUCE 8 Hybrid WebView Plugin Architecture
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PhotoSynthAudioEditor::PhotoSynthAudioEditor (PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Build JUCE 8 WebBrowserComponent Options
    auto options = juce::WebBrowserComponent::Options{}
        .withNativeIntegrationEnabled()
        .withUserScript ("window.__PHOTO_SYNTH_HYBRID__ = true; window.__PHOTO_SYNTH_NATIVE_HOST__ = true;")
        .withResourceProvider ([this](const juce::String& path) {
            return getResource (path);
        })
        // 1. setParameter native function (updates APVTS parameters live from React GUI)
        .withNativeFunction ("setParameter", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 2) {
                juce::String paramId = args[0].toString();
                float value = static_cast<float>(args[1].operator double());

                // Remap frontend names to APVTS IDs if needed
                if (paramId == "cutoffOffset")      paramId = "cutoff";
                else if (paramId == "attackTime")   paramId = "attack";
                else if (paramId == "decayTime")    paramId = "decay";
                else if (paramId == "sustainLevel") paramId = "sustain";
                else if (paramId == "releaseTime")  paramId = "release";
                else if (paramId == "masterVolume") paramId = "volume";
                else if (paramId == "limiterEnabled") paramId = "limiter";

                // Auto-scale normalized UI floats (0.0 - 1.0) to APVTS ranges
                if (paramId == "color" && value <= 1.0f)
                    value *= 360.0f;
                else if ((paramId == "brightness" || paramId == "saturation" || paramId == "complexity") && value <= 1.0f)
                    value *= 100.0f;

                if (auto* param = audioProcessor.apvts.getParameter (paramId)) {
                    param->setValueNotifyingHost (param->convertTo0to1 (value));
                }
            }
            completion (juce::var());
        })
        // 2. syncPatch native function (batch parameter synchronization)
        .withNativeFunction ("syncPatch", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1 && args[0].isObject()) {
                if (auto* obj = args[0].getDynamicObject()) {
                    for (const auto& prop : obj->getProperties()) {
                        juce::String paramId = prop.name.toString();
                        float value = static_cast<float>(prop.value.operator double());

                        if (paramId == "cutoffOffset")      paramId = "cutoff";
                        else if (paramId == "attackTime")   paramId = "attack";
                        else if (paramId == "decayTime")    paramId = "decay";
                        else if (paramId == "sustainLevel") paramId = "sustain";
                        else if (paramId == "releaseTime")  paramId = "release";
                        else if (paramId == "masterVolume") paramId = "volume";
                        else if (paramId == "limiterEnabled") paramId = "limiter";

                        if (paramId == "color" && value <= 1.0f)
                            value *= 360.0f;
                        else if ((paramId == "brightness" || paramId == "saturation" || paramId == "complexity") && value <= 1.0f)
                            value *= 100.0f;

                        if (auto* param = audioProcessor.apvts.getParameter (paramId)) {
                            param->setValueNotifyingHost (param->convertTo0to1 (value));
                        }
                    }
                }
            }
            completion (juce::var());
        })
        // 3. Virtual Keyboard Note On
        .withNativeFunction ("noteOn", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                float vel = args.size() > 1 ? static_cast<float>(args[1].operator double()) : 0.85f;
                audioProcessor.noteOn (note, vel);
            }
            completion (juce::var());
        })
        // 4. Virtual Keyboard Note Off
        .withNativeFunction ("noteOff", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                audioProcessor.noteOff (note);
            }
            completion (juce::var());
        })
        // 5. Native File Chooser for Image Uploading
        .withNativeFunction ("openFileChooser", [this](const juce::Array<juce::var>& args, auto completion) {
            juce::String slot = args.size() >= 1 ? args[0].toString() : "A";
            fileChooser = std::make_unique<juce::FileChooser> (
                "Select Image for Slot " + slot,
                juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                "*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif");

            auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync (folderFlags, [this, slot, completion](const juce::FileChooser& fc) {
                auto resultFile = fc.getResult();
                if (resultFile.existsAsFile()) {
                    juce::Image loadedImg = juce::ImageFileFormat::loadFrom (resultFile);
                    if (loadedImg.isValid()) {
                        if (slot == "A") audioProcessor.loadSlotAImage (loadedImg, resultFile.getFileNameWithoutExtension());
                        else audioProcessor.loadSlotBImage (loadedImg, resultFile.getFileNameWithoutExtension());

                        juce::MemoryOutputStream mos;
                        juce::PNGImageFormat pngFormat;
                        pngFormat.writeImageToStream (loadedImg, mos);
                        juce::String dataUrl = "data:image/png;base64," + juce::Base64::toBase64 (mos.getData(), mos.getDataSize());

                        auto* obj = new juce::DynamicObject();
                        obj->setProperty ("dataUrl", dataUrl);
                        obj->setProperty ("fileName", resultFile.getFileNameWithoutExtension());
                        completion (juce::var (obj));
                        return;
                    }
                }
                completion (juce::var());
            });
        })
        // 6. Sync Slot Image from frontend drag-and-drop
        .withNativeFunction ("syncSlotImage", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 2) {
                juce::String slot = args[0].toString();
                juce::String dataUrl = args[1].toString();
                juce::String name = args.size() >= 3 ? args[2].toString() : juce::String();
                audioProcessor.syncSlotImageFromFrontend (slot, dataUrl, name);
            }
            completion (juce::var());
        })
        // 7. Get Initial State: loads DAW project saved values into the React UI on mount
        .withNativeFunction ("getInitialState", [this](const juce::Array<juce::var>&, auto completion) {
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("bpm", audioProcessor.getBpm());
            obj->setProperty ("isNativeHost", true);
            obj->setProperty ("slotAName", audioProcessor.getSlotAName());
            obj->setProperty ("slotBName", audioProcessor.getSlotBName());
            obj->setProperty ("slotADataUrl", audioProcessor.getImageSlotAAsDataUrl());
            obj->setProperty ("slotBDataUrl", audioProcessor.getImageSlotBAsDataUrl());
            obj->setProperty ("morph", (double)*audioProcessor.apvts.getRawParameterValue ("morph"));

            auto* paramsObj = new juce::DynamicObject();
            for (auto* param : audioProcessor.apvts.processor.getParameters()) {
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*> (param)) {
                    paramsObj->setProperty (p->paramID, (double) p->getValue());
                }
            }
            obj->setProperty ("parameters", juce::var (paramsObj));
            completion (juce::var (obj));
        })
        // 8. Real-time Oscilloscope Waveform Data for React visualizer
        .withNativeFunction ("getWaveformData", [this](const juce::Array<juce::var>& args, auto completion) {
            int numSamples = args.size() >= 1 ? args[0].operator int() : 128;
            numSamples = juce::jlimit (16, 512, numSamples);

            float rawData[512] { 0.0f };
            audioProcessor.getScopeData (rawData, numSamples);

            juce::Array<juce::var> arr;
            for (int i = 0; i < numSamples; ++i) {
                arr.add ((double) rawData[i]);
            }
            completion (juce::var (arr));
        });

    webBrowser = std::make_unique<juce::WebBrowserComponent> (options);
    addAndMakeVisible (*webBrowser);

    // Initial Plugin Window Dimensions
    setResizable (true, true);
    setResizeLimits (960, 600, 1920, 1200);
    setSize (1280, 800);

    // Load web UI
    webBrowser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor() {}

void PhotoSynthAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff09090b));
}

void PhotoSynthAudioEditor::resized()
{
    if (webBrowser != nullptr)
        webBrowser->setBounds (getLocalBounds());
}

std::optional<juce::WebBrowserComponent::Resource> PhotoSynthAudioEditor::getResource (const juce::String& url)
{
    juce::String cleanUrl = url;
    if (cleanUrl.startsWith ("/"))
        cleanUrl = cleanUrl.substring (1);
    if (cleanUrl.isEmpty() || cleanUrl == "/")
        cleanUrl = "index.html";

    // 1. Look in the plugin bundle's Resources directory
    auto bundleDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                        .getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile ("ui");
    auto file = bundleDir.getChildFile (cleanUrl);

    // 2. Dev / Fallback path: look in project dist/ folder
    if (!file.existsAsFile()) {
        auto devDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                         .getChildFile ("dist");
        file = devDir.getChildFile (cleanUrl);
    }

    if (!file.existsAsFile()) {
        // Also check direct dist/ relative to working directory
        file = juce::File::getCurrentWorkingDirectory().getChildFile ("dist").getChildFile (cleanUrl);
    }

    if (file.existsAsFile()) {
        juce::MemoryBlock data;
        file.loadFileAsData (data);

        juce::String mime = "application/octet-stream";
        if (cleanUrl.endsWith (".html")) mime = "text/html";
        else if (cleanUrl.endsWith (".js")) mime = "application/javascript";
        else if (cleanUrl.endsWith (".css")) mime = "text/css";
        else if (cleanUrl.endsWith (".png")) mime = "image/png";
        else if (cleanUrl.endsWith (".jpg") || cleanUrl.endsWith (".jpeg")) mime = "image/jpeg";
        else if (cleanUrl.endsWith (".svg")) mime = "image/svg+xml";
        else if (cleanUrl.endsWith (".wasm")) mime = "application/wasm";

        std::vector<std::byte> byteVec (data.getSize());
        std::memcpy (byteVec.data(), data.getData(), data.getSize());
        return juce::WebBrowserComponent::Resource { std::move (byteVec), mime };
    }

    return std::nullopt;
}
