/*
  ==============================================================================
    WebUIResourceProvider.h
    Reusable JUCE 8 WebBrowserComponent resource provider that serves a web UI
    (Vite "dist") embedded in the binary as a zip (see cmake/EmbedWebUI.cmake).

    Usage:
        options.withResourceProvider ([] (const juce::String& url)
                                      { return webui::getResource (url); });
        webBrowser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    Nothing is read from disk, so the UI can never be blank because of a missing
    bundle folder, a dev server, or an absolute path on the build machine.
  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <mutex>
#include <optional>

#if WEBUI_EMBEDDED
 #include "WebUIBinaryData.h"
#endif

namespace webui
{

inline juce::String mimeTypeFor (const juce::String& path)
{
    const auto ext = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
    static const std::map<juce::String, const char*> types {
        { "html", "text/html" },          { "htm", "text/html" },
        { "js", "text/javascript" },      { "mjs", "text/javascript" },
        { "css", "text/css" },            { "json", "application/json" },
        { "map", "application/json" },    { "svg", "image/svg+xml" },
        { "png", "image/png" },           { "jpg", "image/jpeg" },
        { "jpeg", "image/jpeg" },         { "gif", "image/gif" },
        { "webp", "image/webp" },         { "ico", "image/x-icon" },
        { "woff", "font/woff" },          { "woff2", "font/woff2" },
        { "ttf", "font/ttf" },            { "otf", "font/otf" },
        { "wasm", "application/wasm" },   { "txt", "text/plain" },
        { "mp3", "audio/mpeg" },          { "wav", "audio/wav" },
    };
    const auto it = types.find (ext);
    return it != types.end() ? juce::String (it->second) : juce::String ("application/octet-stream");
}

/** All embedded files, keyed by their path relative to dist/ (e.g. "assets/index-abc.js").
    Decompressed once on first use and cached for the lifetime of the process. */
inline const std::map<juce::String, std::vector<std::byte>>& embeddedFiles()
{
    static std::map<juce::String, std::vector<std::byte>> files;
    static std::once_flag once;
    std::call_once (once, []
    {
       #if WEBUI_EMBEDDED
        juce::MemoryInputStream zipStream (WEBUI_BINARY_NAMESPACE::webui_zip,
                                           (size_t) WEBUI_BINARY_NAMESPACE::webui_zipSize, false);
        juce::ZipFile zip (zipStream);
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            const auto* entry = zip.getEntry (i);
            if (entry == nullptr || entry->filename.endsWithChar ('/'))
                continue;
            std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
            if (in == nullptr)
                continue;
            juce::MemoryBlock block;
            in->readIntoMemoryBlock (block);
            auto name = entry->filename.replaceCharacter ('\\', '/');
            while (name.startsWith ("./")) name = name.substring (2);
            auto& bytes = files[name];
            bytes.resize (block.getSize());
            if (block.getSize() > 0)
                std::memcpy (bytes.data(), block.getData(), block.getSize());
        }
       #endif
    });
    return files;
}

/** Resource provider callback for WebBrowserComponent::Options::withResourceProvider. */
inline std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("#", false, false);
    path = juce::URL::removeEscapeChars (path);
    while (path.startsWith ("/")) path = path.substring (1);
    if (path.isEmpty() || path.endsWith ("/"))
        path += "index.html";

    const auto& files = embeddedFiles();
    auto it = files.find (path);

    // Single-page-app fallback: unknown extension-less routes resolve to index.html.
    if (it == files.end() && ! path.fromLastOccurrenceOf ("/", false, false).containsChar ('.'))
        it = files.find ("index.html");

    if (it == files.end())
        return std::nullopt;

    return juce::WebBrowserComponent::Resource { it->second, mimeTypeFor (it->first) };
}

/** True when the binary contains a usable UI (index.html present). */
inline bool hasEmbeddedUI() { return embeddedFiles().count ("index.html") > 0; }

} // namespace webui
