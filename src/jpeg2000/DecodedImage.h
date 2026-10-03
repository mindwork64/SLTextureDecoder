#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sltcd::jpeg2000 {

/// An image decoded from a JPEG 2000 codestream.
///
/// Samples are 8 bit unsigned and stored interleaved, `components` values per
/// pixel, component 0 first. Subsampled components are rejected, so every
/// component is exactly width x height.
struct DecodedImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t components = 0;
    std::vector<std::uint8_t> pixels;

    std::size_t pixelCount() const noexcept {
        return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    bool empty() const noexcept { return pixels.empty(); }

    /// First sample of the pixel at `index` (row major).
    const std::uint8_t* pixel(std::size_t index) const noexcept { return pixels.data() + index * components; }
};

} // namespace sltcd::jpeg2000
