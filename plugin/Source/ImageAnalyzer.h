/*
  ==============================================================================
    ImageAnalyzer.h
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>

enum class SynthArchetype
{
    AnalogBrass,      // Warm resonant saw stack (Jump-style, Moog/OB-X)
    DigitalFmBells,   // Low-light glassy FM (Little Dark Age / DX7)
    HybridWavetable   // Modern crisp multi-timbral digital
};

enum class TemporalEra
{
    Era1800s,         // Physical, gut strings, cathedral acoustics
    Era1960s,         // Warm vintage tape flutter, organ drawbars
    Era1970s,         // Discrete analog ladder filter, Rhodes
    Era1980s,         // Polyphonic brass, Juno chorus, DX7 FM bells
    Era2000s          // Crisp digital wavetables, high precision
};

struct TemporalEraInfo
{
    TemporalEra era { TemporalEra::Era1970s };
    juce::String eraLabel { "1970s Discrete Analog" };
    juce::String label { "1970s Discrete Analog" };
    juce::String architecture { "Moog Ladder & Fender Rhodes" };
    juce::String description { "Warm, fat discrete analog synths & electric pianos" };
    float eraValue { 0.50f }; // 0.0 to 1.0 continuous spectrum
};

namespace photosynth
{
    using ::SynthArchetype;
    using ::TemporalEra;
    using ::TemporalEraInfo;

    inline juce::String toEngineParamText(SynthArchetype archetype)
    {
        switch (archetype)
        {
            case SynthArchetype::AnalogBrass:
                return "Analog Polyphonic Brass (Moog/OB-X Stack)";
            case SynthArchetype::DigitalFmBells:
                return "2-Op Glassy FM Bells (DX7 Spec)";
            case SynthArchetype::HybridWavetable:
            default:
                return "High-Precision Digital Wavetable";
        }
    }
}

inline juce::String toEngineParamText(SynthArchetype archetype)
{
    return photosynth::toEngineParamText(archetype);
}

struct ImageAnalysisResult
{
    juce::uint64 pixelSeed { 0 };
    juce::String seedHex;
    float brightness { 0.5f };   // Luminance 0.0 - 1.0
    float saturation { 0.5f };   // Saturation 0.0 - 1.0
    float complexity { 0.5f };   // Sobel Edge Density 0.0 - 1.0
    float timbreDna  { 0.5f };   // Hue Spectrum Variance 0.0 - 1.0
    float earthyMutedness { 0.3f };
    float colorEntropy { 0.5f };

    SynthArchetype archetype { SynthArchetype::AnalogBrass };
    TemporalEraInfo temporalEra;

    // Derived Audio Synthesis Parameters matching Web Prototype ImageAnalyzer
    float cutoff { 2000.0f };
    float resonance { 1.0f };
    float attack { 0.01f };
    float decay { 0.5f };
    float sustain { 0.7f };
    float release { 0.3f };
    float lfoRate { 2.0f };
    float lfoDepth { 0.2f };
    float drive { 0.0f };
    float delayMix { 0.2f };
    float reverbMix { 0.3f };
};

class ImageAnalyzer
{
public:
    ImageAnalyzer() = default;
    ~ImageAnalyzer() = default;

    /** Analyzes image deterministically using juce::Image::BitmapData */
    static ImageAnalysisResult analyzeImage(const juce::Image& image);

    /** Linearly interpolates two image analysis results based on morph factor t (0.0 to 1.0) */
    static ImageAnalysisResult interpolate(const ImageAnalysisResult& a, const ImageAnalysisResult& b, float t)
    {
        auto lerp = [](float v1, float v2, float factor) { return v1 + (v2 - v1) * factor; };
        ImageAnalysisResult res;
        res.brightness = lerp(a.brightness, b.brightness, t);
        res.saturation = lerp(a.saturation, b.saturation, t);
        res.complexity = lerp(a.complexity, b.complexity, t);
        res.timbreDna  = lerp(a.timbreDna,  b.timbreDna,  t);
        res.earthyMutedness = lerp(a.earthyMutedness, b.earthyMutedness, t);
        res.colorEntropy    = lerp(a.colorEntropy,    b.colorEntropy,    t);
        res.cutoff     = lerp(a.cutoff,     b.cutoff,     t);
        res.resonance  = lerp(a.resonance,  b.resonance,  t);
        res.attack     = lerp(a.attack,     b.attack,     t);
        res.decay      = lerp(a.decay,      b.decay,      t);
        res.sustain    = lerp(a.sustain,    b.sustain,    t);
        res.release    = lerp(a.release,    b.release,    t);
        res.drive      = lerp(a.drive,      b.drive,      t);
        res.delayMix   = lerp(a.delayMix,   b.delayMix,   t);
        res.reverbMix  = lerp(a.reverbMix,  b.reverbMix,  t);
        res.archetype  = t < 0.5f ? a.archetype : b.archetype;
        res.temporalEra = t < 0.5f ? a.temporalEra : b.temporalEra;
        res.pixelSeed  = t < 0.5f ? a.pixelSeed : b.pixelSeed;
        res.seedHex    = t < 0.5f ? a.seedHex : b.seedHex;
        return res;
    }

    /** Blends two images with opacity crossfade */
    static juce::Image createBlendedPreview(const juce::Image& imgA, const juce::Image& imgB, float t)
    {
        if (!imgA.isValid()) return imgB;
        if (!imgB.isValid()) return imgA;
        int w = juce::jmin(imgA.getWidth(), imgB.getWidth());
        int h = juce::jmin(imgA.getHeight(), imgB.getHeight());
        if (w <= 0 || h <= 0) return imgA;

        juce::Image blended(juce::Image::RGB, w, h, true);
        juce::Graphics g(blended);
        g.drawImage(imgA, 0, 0, w, h, 0, 0, imgA.getWidth(), imgA.getHeight());
        g.setOpacity(juce::jlimit(0.0f, 1.0f, t));
        g.drawImage(imgB, 0, 0, w, h, 0, 0, imgB.getWidth(), imgB.getHeight());
        return blended;
    }

    /** Computes 64-bit FNV-1a checksum across entire image bitmap */
    static juce::uint64 computeFNV1a64(const juce::Image::BitmapData& bitmap);

private:
    static float computeSobelEdgeDensity(const juce::Image::BitmapData& bitmap);
    static TemporalEraInfo computeTemporalEra(
        float brightness,
        float saturation,
        float complexity,
        float earthyMutedness,
        float colorEntropy,
        float rRatio, float gRatio, float bRatio,
        float cRatio, float mRatio, float yRatio
    );

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImageAnalyzer)
};
