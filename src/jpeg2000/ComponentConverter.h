#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "jpeg2000/DecodedImage.h"

namespace sltcd::jpeg2000 {

/// Turns decoded component samples into the one pixel format the rest of the
/// tool works with: interleaved, 8 bit RGBA.
///
/// Mapping policy (see docs/format-notes.md):
///
///   1 component  -> luminance, opaque
///   2 components -> luminance + alpha
///   3 components -> RGB, opaque
///   4 components -> RGBA
///   more         -> RGBA from the first four components; the surplus planes are
///                   dropped and `warning` (when not null) receives a note
///
/// Throws sltcd::DecodeError when `image` has no pixels or inconsistent sizes.
std::vector<std::uint8_t> toRgba(const DecodedImage& image, std::string* warning = nullptr);

} // namespace sltcd::jpeg2000
