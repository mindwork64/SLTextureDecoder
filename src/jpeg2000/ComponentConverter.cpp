#include "jpeg2000/ComponentConverter.h"

#include <cstddef>
#include <string>

#include "utils/Errors.h"

namespace sltcd::jpeg2000 {
namespace {

constexpr std::size_t kRgbChannels = 3;
constexpr std::size_t kRgbaChannels = 4;

/// Shared body of toRgba/toRgb. `outChannels` is 4 for RGBA and 3 for RGB; the
/// alpha channel is only written when the output format has one.
std::vector<std::uint8_t> convert(const DecodedImage& image, std::size_t outChannels, std::string* warning) {
    const std::size_t pixels = image.pixelCount();
    const std::size_t components = image.components;
    if (pixels == 0 || components == 0) {
        throw DecodeError("cannot convert an image without pixels");
    }
    if (image.pixels.size() != pixels * components) {
        throw DecodeError("decoded image holds " + std::to_string(image.pixels.size()) + " samples for " +
                          std::to_string(pixels) + " pixels with " + std::to_string(components) + " components");
    }

    // Alpha defaults to opaque, so the branches that ignore alpha are just
    // copies of the colour samples.
    std::vector<std::uint8_t> output(pixels * outChannels, 255);
    const std::uint8_t* source = image.pixels.data();
    std::uint8_t* target = output.data();
    const bool withAlpha = outChannels == kRgbaChannels;

    for (std::size_t i = 0; i < pixels; ++i, source += components, target += outChannels) {
        switch (components) {
        case 1: // luminance
            target[0] = target[1] = target[2] = source[0];
            break;
        case 2: // luminance + alpha
            target[0] = target[1] = target[2] = source[0];
            if (withAlpha) {
                target[3] = source[1];
            }
            break;
        case 3: // RGB, opaque
            target[0] = source[0];
            target[1] = source[1];
            target[2] = source[2];
            break;
        default: // RGBA; codestreams with 5 components carry a surplus plane
            target[0] = source[0];
            target[1] = source[1];
            target[2] = source[2];
            if (withAlpha) {
                target[3] = source[3];
            }
            break;
        }
    }

    if (components > kRgbaChannels && warning != nullptr) {
        *warning = "codestream carries " + std::to_string(components) +
                   " components; the surplus planes were dropped (see docs/format-notes.md)";
    }
    return output;
}

} // namespace

std::vector<std::uint8_t> toRgba(const DecodedImage& image, std::string* warning) {
    return convert(image, kRgbaChannels, warning);
}

std::vector<std::uint8_t> toRgb(const DecodedImage& image, std::string* warning) {
    return convert(image, kRgbChannels, warning);
}

} // namespace sltcd::jpeg2000
