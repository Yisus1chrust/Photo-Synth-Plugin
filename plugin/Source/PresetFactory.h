/*
  ==============================================================================
    PresetFactory.h
    Modular Sound Technologies - Photo Synth
    Built-in procedural image synthesizers for factory presets
  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace photosynth
{
    class PresetFactory
    {
    public:
        static juce::Image createSynthwaveSunset(int w = 400, int h = 300)
        {
            juce::Image img(juce::Image::RGB, w, h, true);
            juce::Graphics g(img);

            // Sky gradient: Pink -> Amber -> Deep Violet
            juce::ColourGradient skyGrad(
                juce::Colour(0xffff0055), 0, 0,
                juce::Colour(0xff110022), 0, (float)h,
                false
            );
            skyGrad.addColour(0.45, juce::Colour(0xffffaa00));
            g.setGradientFill(skyGrad);
            g.fillAll();

            // Glowing sun
            float sunX = w * 0.5f;
            float sunY = h * 0.42f;
            float sunR = h * 0.28f;
            juce::ColourGradient sunGrad(
                juce::Colour(0xffffff00), sunX, sunY,
                juce::Colour(0x00ff00aa), sunX, sunY + sunR,
                true
            );
            sunGrad.addColour(0.7, juce::Colour(0xffff00aa));
            g.setGradientFill(sunGrad);
            g.fillEllipse(sunX - sunR, sunY - sunR, sunR * 2.0f, sunR * 2.0f);

            // Horizon & Cyan perspective grid
            float horizY = h * 0.58f;
            g.setColour(juce::Colour(0xff090014));
            g.fillRect(0.0f, horizY, (float)w, (float)(h - horizY));

            g.setColour(juce::Colour(0xff00ffff));
            for (int x = -w; x <= w * 2; x += 35)
            {
                g.drawLine(x, (float)h, sunX, horizY, 1.5f);
            }
            for (float y = horizY; y <= h; y += (y - horizY) * 0.4f + 8.0f)
            {
                g.drawLine(0.0f, y, (float)w, y, 1.5f);
            }

            return img;
        }

        static juce::Image createObsidianCrypt(int w = 400, int h = 300)
        {
            juce::Image img(juce::Image::RGB, w, h, true);
            juce::Graphics g(img);

            // Dark moody radial backdrop
            juce::ColourGradient bg(
                juce::Colour(0xff1a2238), w * 0.5f, h * 0.5f,
                juce::Colour(0xff020305), 0, 0,
                true
            );
            bg.addColour(0.6, juce::Colour(0xff0b0e14));
            g.setGradientFill(bg);
            g.fillAll();

            // Glassy crystalline shards
            g.setColour(juce::Colour(0x5564b4ff));
            for (int i = 0; i < 18; ++i)
            {
                float cx = (std::sin(i * 1.5f) * 0.38f + 0.5f) * w;
                float cy = (std::cos(i * 1.1f) * 0.38f + 0.5f) * h;
                float sz = 24.0f + i * 9.0f;
                g.drawRect(cx - sz * 0.5f, cy - sz * 0.5f, sz, sz, 1.2f);
                g.drawLine(cx, cy - sz * 0.5f, cx, cy + sz * 0.5f, 0.8f);
            }

            return img;
        }

        static juce::Image createRefractivePrism(int w = 400, int h = 300)
        {
            juce::Image img(juce::Image::RGB, w, h, true);
            juce::Graphics g(img);

            g.setColour(juce::Colour(0xff0a0a10));
            g.fillAll();

            // Spectral rainbow dispersion triangles
            std::vector<juce::Colour> cols = {
                juce::Colour(0xffff0055),
                juce::Colour(0xff7a00ff),
                juce::Colour(0xff00d2ff),
                juce::Colour(0xff00ffaa),
                juce::Colour(0xffffea00)
            };

            for (size_t i = 0; i < cols.size(); ++i)
            {
                juce::Path p;
                p.startNewSubPath(w * 0.5f, h * 0.2f);
                float x1 = w * 0.15f + i * (w * 0.15f);
                float x2 = x1 + w * 0.12f;
                p.lineTo(x1, (float)h * 0.85f);
                p.lineTo(x2, (float)h * 0.85f);
                p.closeSubPath();

                g.setColour(cols[i].withAlpha(0.65f));
                g.fillPath(p);
                g.setColour(cols[i]);
                g.strokePath(p, juce::PathStrokeType(1.5f));
            }

            return img;
        }

        static juce::Image createNeonAurora(int w = 400, int h = 300)
        {
            juce::Image img(juce::Image::RGB, w, h, true);
            juce::Graphics g(img);

            // Night sky
            g.setColour(juce::Colour(0xff040914));
            g.fillAll();

            // Wavy aurora ribbon
            for (int pass = 0; pass < 3; ++pass)
            {
                juce::Path aurora;
                float baseY = h * (0.35f + pass * 0.15f);
                aurora.startNewSubPath(0, baseY);

                for (int x = 0; x <= w; x += 20)
                {
                    float y = baseY + std::sin(x * 0.02f + pass * 1.5f) * 35.0f;
                    aurora.lineTo((float)x, y);
                }
                aurora.lineTo((float)w, (float)h);
                aurora.lineTo(0.0f, (float)h);
                aurora.closeSubPath();

                juce::Colour auroraCol = (pass % 2 == 0) ? juce::Colour(0x6600ffaa) : juce::Colour(0x6606b6d4);
                g.setColour(auroraCol);
                g.fillPath(aurora);
            }

            return img;
        }

        static juce::Image createBioluminescentCoral(int w = 400, int h = 300) { return createNeonAurora(w, h); }
        static juce::Image createDesertDunes(int w = 400, int h = 300) { return createRefractivePrism(w, h); }
        static juce::Image createCyberpunkAlley(int w = 400, int h = 300) { return createSynthwaveSunset(w, h); }

        static juce::Image getPresetImage(int index, int w = 400, int h = 300)
        {
            switch (index % 4)
            {
                case 0: return createSynthwaveSunset(w, h);
                case 1: return createObsidianCrypt(w, h);
                case 2: return createRefractivePrism(w, h);
                case 3: return createNeonAurora(w, h);
                default: return createSynthwaveSunset(w, h);
            }
        }

        static juce::String getPresetName(int index)
        {
            switch (index % 4)
            {
                case 0: return "Synthwave Sunset";
                case 1: return "Obsidian Crypt";
                case 2: return "Refractive Prism";
                case 3: return "Neon Aurora";
                default: return "Synthwave Sunset";
            }
        }
    };
}
