/*
  ==============================================================================
    PluginEditor.cpp
    Modular Sound Technologies - Paint a Synth (Photo Synth)
    JUCE 8 Hybrid WebView Plugin Architecture
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebUIResourceProvider.h"

PhotoSynthAudioEditor::PhotoSynthAudioEditor (PhotoSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Build JUCE 8 WebBrowserComponent Options
    auto options = juce::WebBrowserComponent::Options{}
       #if JUCE_WINDOWS
        // Edge/Chromium WebView2 (statically linked loader) instead of the legacy IE engine.
        // A per-user writable data folder avoids failures inside read-only plugin folders.
        .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                     .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                              .getChildFile ("PhotoSynthWebView2")))
       #endif
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withUserScript ("window.__PHOTO_SYNTH_HYBRID__ = true; window.__PHOTO_SYNTH_NATIVE_HOST__ = true;")
        // The React UI is embedded in the binary (cmake/EmbedWebUI.cmake) - no disk/dev-server access.
        .withResourceProvider ([](const juce::String& path) {
            return webui::getResource (path);
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

                // Values always arrive in APVTS units (no "<= 1 means normalised" guessing,
                // which turned e.g. brightness 0.8 % into 80 %).
                if (std::isfinite (value))
                    if (auto* param = audioProcessor.apvts.getParameter (paramId))
                        param->setValueNotifyingHost (param->convertTo0to1 (value));
                // UI-originated: do not echo it back as a host change.
                snapshotParameters();
            }
            completion (juce::var());
        })
        // 2. syncPatch native function (batch parameter synchronization of the
        //    photo-derived patch). Logic lives in the processor so it is unit-testable.
        .withNativeFunction ("syncPatch", [this](const juce::Array<juce::var>& args, auto completion) {
            const int numSet = args.size() >= 1 ? audioProcessor.applyFrontendPatch (args[0]) : 0;
            snapshotParameters();
            completion (juce::var (numSet));
        })
        // 3. Virtual Keyboard Note On
        .withNativeFunction ("noteOn", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                float vel = args.size() > 1 ? static_cast<float>(args[1].operator double()) : 0.85f;
                int ch = args.size() > 2 ? args[2].operator int() : 1;
                audioProcessor.noteOn (note, vel, ch);
            }
            completion (juce::var());
        })
        // 4. Virtual Keyboard Note Off
        .withNativeFunction ("noteOff", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1) {
                int note = args[0].operator int();
                int ch = args.size() > 1 ? args[1].operator int() : 1;
                audioProcessor.noteOff (note, ch);
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
                completion (juce::var (audioProcessor.syncSlotImageFromFrontend (slot, dataUrl, name)));
                return;
            }
            completion (juce::var (false));
        })
        // 7. Get state: the UI hydrates from this on every page load (editor reopen,
        //    track switch, project load) instead of starting from its defaults.
        .withNativeFunction ("getInitialState", [this](const juce::Array<juce::var>&, auto completion) {
            completion (audioProcessor.getFullStateForFrontend());
        })
        .withNativeFunction ("getState", [this](const juce::Array<juce::var>&, auto completion) {
            completion (audioProcessor.getFullStateForFrontend());
        })
        // 7b. UI-only state blob (JSON text or object). Stored in the processor and saved
        //     with the DAW project; never triggers a "stateRestored" echo.
        .withNativeFunction ("setUiState", [this](const juce::Array<juce::var>& args, auto completion) {
            if (args.size() >= 1)
                audioProcessor.setUiState (args[0].isString() ? args[0].toString()
                                                              : juce::JSON::toString (args[0], true));
            completion (juce::var (true));
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

    // Host restores (preset / project / undo) while the editor is open -> push to UI.
    audioProcessor.addChangeListener (this);

    // Host automation / generic-editor changes -> push to UI (polled, message thread).
    for (auto* param : audioProcessor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            trackedParams.push_back (ranged);
    lastSentValues.resize (trackedParams.size(), 0.0f);
    snapshotParameters();
    startTimerHz (20);

    // Load the embedded web UI
    jassert (webui::hasEmbeddedUI());   // dist was not embedded at build time
    webBrowser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

PhotoSynthAudioEditor::~PhotoSynthAudioEditor()
{
    stopTimer();
    audioProcessor.removeChangeListener (this);
}

void PhotoSynthAudioEditor::snapshotParameters()
{
    for (size_t i = 0; i < trackedParams.size(); ++i)
        lastSentValues[i] = trackedParams[i]->getValue();
}

void PhotoSynthAudioEditor::timerCallback()
{
    if (webBrowser == nullptr)
        return;

    for (size_t i = 0; i < trackedParams.size(); ++i)
    {
        const float norm = trackedParams[i]->getValue();
        if (std::abs (norm - lastSentValues[i]) > 1.0e-6f)
        {
            lastSentValues[i] = norm;
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("id", trackedParams[i]->paramID);
            obj->setProperty ("value", (double) trackedParams[i]->convertFrom0to1 (norm));
            webBrowser->emitEventIfBrowserIsVisible ("parameterChanged", juce::var (obj));
        }
    }
}

void PhotoSynthAudioEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // setStateInformation ran (host project/preset load). Everything changed at once:
    // send one full-state event instead of dozens of parameter events.
    snapshotParameters();
    if (webBrowser != nullptr)
        webBrowser->emitEventIfBrowserIsVisible ("stateRestored", audioProcessor.getFullStateForFrontend());
}

void PhotoSynthAudioEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff09090b));
}

void PhotoSynthAudioEditor::resized()
{
    if (webBrowser != nullptr)
        webBrowser->setBounds (getLocalBounds());
}
