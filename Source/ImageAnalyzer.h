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
};

class ImageAnalyzer
{
public:
    ImageAnalyzer() = default;
    ~ImageAnalyzer() = default;

    /** Analyzes image deterministically using juce::Image::BitmapData */
    static ImageAnalysisResult analyzeImage(const juce::Image& image);

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
