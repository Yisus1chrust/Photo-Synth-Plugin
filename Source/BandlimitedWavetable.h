/*
  ==============================================================================
    BandlimitedWavetable.h
    Modular Sound Technologies - Photo Synth

    Mip-mapped, band-limited single-cycle wavetables (one table per octave).
    Each mip level holds only the harmonics that stay below Nyquist for the
    highest fundamental it is used for, so playback is alias-free at any pitch.

    Tables are built ONCE (function-local static, thread-safe init) from the
    message thread when the engine is constructed; the audio thread only reads.
  ==============================================================================
*/

#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace photosynth
{

class BandlimitedWavetable
{
public:
    static constexpr int tableSize = 2048;              // samples per cycle (power of two)
    static constexpr int numLevels = 11;                // level l: phase increment <= 2^l / tableSize
    static constexpr int maxHarmonics = tableSize / 2 - 1;

    enum class Shape { saw, square };

    /** Shared, immutable tables. First call builds them (do this off the audio thread). */
    static const BandlimitedWavetable& saw()    { static const BandlimitedWavetable t (Shape::saw);    return t; }
    static const BandlimitedWavetable& square() { static const BandlimitedWavetable t (Shape::square); return t; }

    /** Mip level for a normalised phase increment (cycles per sample). Realtime-safe. */
    static inline int levelFor (float phaseIncrement) noexcept
    {
        const float x = phaseIncrement * (float) tableSize;   // 2^level must be >= x
        int level = 0;
        float limit = 1.0f;
        while (level < numLevels - 1 && limit < x) { limit *= 2.0f; ++level; }
        return level;
    }

    /** phase in [0,1). Linear interpolation inside the alias-free mip level. Realtime-safe. */
    inline float lookup (float phase, int level) const noexcept
    {
        const float* t = tables[(size_t) level].data();
        const float pos = phase * (float) tableSize;
        const int i0 = (int) pos & (tableSize - 1);
        const float frac = pos - std::floor (pos);
        return t[i0] + frac * (t[i0 + 1] - t[i0]);          // table has a guard sample
    }

    inline float process (float phase, float phaseIncrement) const noexcept
    {
        return lookup (phase, levelFor (phaseIncrement));
    }

private:
    explicit BandlimitedWavetable (Shape shape)
    {
        constexpr double twoPi = 6.283185307179586;
        std::vector<double> sinTable ((size_t) tableSize);
        for (int n = 0; n < tableSize; ++n)
            sinTable[(size_t) n] = std::sin (twoPi * n / tableSize);

        for (int level = 0; level < numLevels; ++level)
        {
            // Highest fundamental for this level is 2^level / tableSize cycles/sample.
            const int harmonicsBelowNyquist = (tableSize / 2) >> level;
            const int numHarmonics = harmonicsBelowNyquist < 1 ? 1
                                   : (harmonicsBelowNyquist > maxHarmonics ? maxHarmonics : harmonicsBelowNyquist);

            auto& table = tables[(size_t) level];
            for (int n = 0; n < tableSize; ++n)
            {
                double v = 0.0;
                for (int h = 1; h <= numHarmonics; ++h)
                {
                    if (shape == Shape::square && (h % 2) == 0)
                        continue;
                    const double s = sinTable[(size_t) ((h * n) & (tableSize - 1))];
                    // saw: matches the naive ramp 2p-1 ; square: +1 for p < 0.5, -1 otherwise
                    v += (shape == Shape::saw ? -2.0 / (3.141592653589793 * h)
                                              :  4.0 / (3.141592653589793 * h)) * s;
                }
                table[(size_t) n] = (float) v;
            }
            table[(size_t) tableSize] = table[0];
        }
    }

    std::array<std::array<float, tableSize + 1>, numLevels> tables {};
};

} // namespace photosynth
