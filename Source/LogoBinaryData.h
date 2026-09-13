#pragma once
#include <cstddef>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace photosynth {
class LogoBinaryData {
public:
    static const unsigned char* getLogoData();
    static size_t getLogoSize();
    static juce::Image getLogoImage();
};
}
