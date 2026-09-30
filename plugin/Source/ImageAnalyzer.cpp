/*
  ==============================================================================
    ImageAnalyzer.cpp
    Modular Sound Technologies - Photo Synth
  ==============================================================================
*/

#include "ImageAnalyzer.h"
#include <cmath>
#include <algorithm>

juce::uint64 ImageAnalyzer::computeFNV1a64(const juce::Image::BitmapData& bitmap)
{
    const juce::uint64 fnvOffsetBasis = 14695981039346656037ULL;
    const juce::uint64 fnvPrime       = 1099511628211ULL;

    juce::uint64 hash = fnvOffsetBasis;

    for (int y = 0; y < bitmap.height; ++y)
    {
        for (int x = 0; x < bitmap.width; ++x)
        {
            auto pixel = bitmap.getPixelColour(x, y);
            hash ^= pixel.getRed();   hash *= fnvPrime;
            hash ^= pixel.getGreen(); hash *= fnvPrime;
            hash ^= pixel.getBlue();  hash *= fnvPrime;
        }
    }
    return hash;
}

float ImageAnalyzer::computeSobelEdgeDensity(const juce::Image::BitmapData& bitmap)
{
    float edgeSum = 0.0f;
    int w = bitmap.width;
    int h = bitmap.height;
    if (w < 3 || h < 3) return 0.2f;

    for (int y = 1; y < h - 1; ++y)
    {
        for (int x = 1; x < w - 1; ++x)
        {
            auto getL = [&](int px, int py) {
                auto c = bitmap.getPixelColour(px, py);
                return 0.299f * c.getFloatRed() + 0.587f * c.getFloatGreen() + 0.114f * c.getFloatBlue();
            };

            float gx = -getL(x-1,y-1) + getL(x+1,y-1) - 2.0f*getL(x-1,y) + 2.0f*getL(x+1,y) - getL(x-1,y+1) + getL(x+1,y+1);
            float gy = -getL(x-1,y-1) - 2.0f*getL(x,y-1) - getL(x+1,y-1) + getL(x-1,y+1) + 2.0f*getL(x,y+1) + getL(x+1,y+1);

            float mag = std::sqrt(gx * gx + gy * gy);
            if (mag > 0.2f) edgeSum += mag;
        }
    }
    return juce::jlimit(0.0f, 1.0f, edgeSum / ((w - 2) * (h - 2) * 0.4f));
}

TemporalEraInfo ImageAnalyzer::computeTemporalEra(
    float brightness,
    float saturation,
    float complexity,
    float earthyMutedness,
    float colorEntropy,
    float rRatio, float /*gRatio*/, float /*bRatio*/,
    float cRatio, float mRatio, float yRatio
)
{
    // Era 1: 1800s & Prior (Physical, Patina, Cathedral)
    float score1800s = std::max(0.0f, earthyMutedness * 0.50f + (1.0f - saturation) * 0.30f + (1.0f - brightness) * 0.20f + yRatio * 0.15f);

    // Era 2: 1960s (Warm tape, amber/yellow tones, drawbar organ)
    float score1960s = std::max(0.0f, (yRatio * 0.45f + rRatio * 0.25f) + (1.0f - std::abs(saturation - 0.35f)) * 0.30f);

    // Era 3: 1970s (Discrete analog, Moog ladder filter, Rhodes)
    float score1970s = std::max(0.0f, (rRatio * 0.35f + mRatio * 0.20f) + (1.0f - std::abs(saturation - 0.45f)) * 0.25f + (1.0f - std::abs(brightness - 0.50f)) * 0.20f);

    // Era 4: 1980s (Vibrant neon, Oberheim OB-X, DX7 FM bells)
    float primarySecondaryPop = cRatio + mRatio + yRatio;
    float score1980s = std::max(0.0f, saturation * 0.55f + colorEntropy * 0.30f + primarySecondaryPop * 0.25f);

    // Era 5: 1990s-2000s+ (Crisp digital, high edge sharpness, pristine wavetables)
    float score2000s = std::max(0.0f, complexity * 0.45f + (1.0f - earthyMutedness) * 0.35f + (brightness > 0.7f ? 0.2f : 0.0f));

    struct EraScore { TemporalEra era; float score; float center; const char* label; const char* arch; const char* desc; };
    EraScore scores[] = {
        { TemporalEra::Era1800s, score1800s, 0.05f, "1800s Physical & Cathedral", "Harpsichord, Gut Lute & Cathedral Organ", "Acoustic physical modeling, pure harmonics & cathedral reverb" },
        { TemporalEra::Era1960s, score1960s, 0.25f, "1960s Vintage Electric", "Hammond/Vox Drawbar Organ & Spring", "Psychedelic drawbar tonewheels, combo organs, tape flutter & spring reverb" },
        { TemporalEra::Era1970s, score1970s, 0.50f, "1970s Discrete Analog", "Moog Ladder & Fender Rhodes", "Warm, fat discrete analog synths, ARP 2600 modular screams & electric pianos" },
        { TemporalEra::Era1980s, score1980s, 0.75f, "1980s Polyphonic OB-X & DX7", "Oberheim OB-X & DX7 FM Bells", "Lush brass polyphony, iconic Roland Juno chorus & crystalline FM electric keys" },
        { TemporalEra::Era2000s, score2000s, 0.95f, "1990s-2000s+ Digital Wavetable", "Access Virus, JP-8000 & Wavetable", "Hyper-saw unisons, razor-sharp digital precision, formant pads & glassy wavetables" }
    };

    std::sort(std::begin(scores), std::end(scores), [](const EraScore& a, const EraScore& b) { return a.score > b.score; });
    const auto& top = scores[0];

    TemporalEraInfo info;
    info.era = top.era;
    info.eraLabel = top.label;
    info.label = top.label;
    info.architecture = top.arch;
    info.description = top.desc;
    info.eraValue = top.center;

    return info;
}

ImageAnalysisResult ImageAnalyzer::analyzeImage(const juce::Image& image)
{
    ImageAnalysisResult result;
    if (image.isNull()) return result;

    // Rescale down to max 256 for instantaneous evaluation (< 1ms)
    juce::Image evalImage = image;
    const int maxDim = 256;
    if (image.getWidth() > maxDim || image.getHeight() > maxDim) {
        int w = image.getWidth();
        int h = image.getHeight();
        if (w > h) {
            h = std::max(1, (h * maxDim) / w);
            w = maxDim;
        } else {
            w = std::max(1, (w * maxDim) / h);
            h = maxDim;
        }
        evalImage = image.rescaled(w, h, juce::Graphics::lowResamplingQuality);
    }

    juce::Image::BitmapData bitmap(evalImage, juce::Image::BitmapData::readOnly);

    // 1. Calculate 64-Bit FNV-1a Checksum Seed
    result.pixelSeed = computeFNV1a64(bitmap);
    result.seedHex = "0x" + juce::String::toHexString((juce::int64)result.pixelSeed).toUpperCase();

    // 2. Luminance, Saturation, Hue Breakdown
    double totalLum = 0.0;
    double totalSat = 0.0;
    int totalPixels = bitmap.width * bitmap.height;

    int countR = 0, countG = 0, countB = 0;
    int countC = 0, countM = 0, countY = 0;
    int countMuted = 0;

    for (int y = 0; y < bitmap.height; ++y)
    {
        for (int x = 0; x < bitmap.width; ++x)
        {
            auto color = bitmap.getPixelColour(x, y);
            float r = color.getFloatRed();
            float g = color.getFloatGreen();
            float b = color.getFloatBlue();

            float lum = 0.299f * r + 0.587f * g + 0.114f * b;
            totalLum += lum;

            float maxC = juce::jmax(r, juce::jmax(g, b));
            float minC = juce::jmin(r, juce::jmin(g, b));
            float delta = maxC - minC;
            float sat = (maxC > 0.001f) ? (delta / maxC) : 0.0f;
            totalSat += sat;

            if (sat < 0.25f && lum > 0.15f && lum < 0.75f) {
                countMuted++;
            }

            if (delta > 0.08f) {
                float hue = 0.0f;
                if (maxC == r) hue = std::fmod((g - b) / delta, 6.0f);
                else if (maxC == g) hue = (b - r) / delta + 2.0f;
                else hue = (r - g) / delta + 4.0f;
                hue = std::round(hue * 60.0f);
                if (hue < 0.0f) hue += 360.0f;

                if (hue >= 345.0f || hue < 25.0f) countR++;
                else if (hue >= 25.0f && hue < 65.0f) countY++;
                else if (hue >= 65.0f && hue < 160.0f) countG++;
                else if (hue >= 160.0f && hue < 210.0f) countC++;
                else if (hue >= 210.0f && hue < 275.0f) countB++;
                else if (hue >= 275.0f && hue < 345.0f) countM++;
            }
        }
    }

    result.brightness = juce::jlimit(0.0f, 1.0f, (float)(totalLum / totalPixels));
    result.saturation = juce::jlimit(0.0f, 1.0f, (float)(totalSat / totalPixels));
    result.complexity = computeSobelEdgeDensity(bitmap);
    result.earthyMutedness = (float)countMuted / (float)totalPixels;

    float rRatio = (float)countR / totalPixels;
    float gRatio = (float)countG / totalPixels;
    float bRatio = (float)countB / totalPixels;
    float cRatio = (float)countC / totalPixels;
    float mRatio = (float)countM / totalPixels;
    float yRatio = (float)countY / totalPixels;

    // Color entropy calculation
    float ratios[] = { rRatio, gRatio, bRatio, cRatio, mRatio, yRatio };
    float entropy = 0.0f;
    for (float p : ratios) {
        if (p > 0.001f) entropy -= p * std::log2(p);
    }
    result.colorEntropy = juce::jlimit(0.0f, 1.0f, entropy / 2.585f);
    result.timbreDna = juce::jlimit(0.0f, 1.0f, result.saturation * 0.5f + result.colorEntropy * 0.3f + result.complexity * 0.2f);

    // Archetype selection
    if (result.saturation > 0.40f && result.brightness > 0.35f) {
        result.archetype = SynthArchetype::AnalogBrass;
    } else if (result.brightness < 0.45f || result.saturation < 0.25f) {
        result.archetype = SynthArchetype::DigitalFmBells;
    } else {
        result.archetype = SynthArchetype::HybridWavetable;
    }

    // Temporal Era Calculation
    result.temporalEra = computeTemporalEra(
        result.brightness,
        result.saturation,
        result.complexity,
        result.earthyMutedness,
        result.colorEntropy,
        rRatio, gRatio, bRatio,
        cRatio, mRatio, yRatio
    );

    // Audio Synthesis Parameter Extraction (1:1 with Web Prototype ImageAnalyzer)
    result.cutoff    = juce::jlimit(120.0f, 19500.0f, 150.0f + std::pow(result.brightness, 1.8f) * 16000.0f + result.complexity * 3200.0f);
    result.resonance = juce::jlimit(0.1f, 10.0f, 0.5f + result.saturation * 6.5f + result.colorEntropy * 2.5f);
    result.attack    = juce::jlimit(0.002f, 3.5f, 0.002f + std::pow(1.0f - result.complexity, 2.5f) * 1.5f);
    result.decay     = juce::jlimit(0.05f, 6.0f, 0.08f + (1.0f - result.complexity * 0.6f) * 2.0f);
    result.sustain   = juce::jlimit(0.05f, 0.95f, 0.15f + (1.0f - result.complexity * 0.8f) * 0.6f);
    result.release   = juce::jlimit(0.01f, 5.0f, 0.05f + std::pow(1.0f - result.saturation, 1.8f) * 1.5f);
    result.lfoRate   = juce::jlimit(0.1f, 20.0f, 0.2f + result.complexity * 12.0f);
    result.lfoDepth  = juce::jlimit(0.0f, 1.0f, 0.15f + result.saturation * 0.55f);
    result.drive     = result.saturation > 0.4f ? (result.saturation - 0.4f) * 1.5f : 0.0f;
    result.delayMix  = juce::jlimit(0.0f, 1.0f, 0.10f + result.complexity * 0.45f + cRatio * 0.25f);
    result.reverbMix = juce::jlimit(0.0f, 1.0f, 0.10f + (1.0f - result.brightness) * 0.50f + result.earthyMutedness * 0.30f);

    return result;
}
