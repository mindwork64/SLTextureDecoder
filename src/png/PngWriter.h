#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace sltcd::png {

/// Write an 8 bit RGBA PNG (non interlaced, no ancillary chunks).
///
/// `rgba` must hold exactly width * height * 4 samples, row major. The file is
/// removed again when libpng fails, so no half written PNG is left behind.
/// Throws sltcd::WriteError on any failure.
void writeRgba(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba, std::uint32_t width,
               std::uint32_t height);

/// Write an 8 bit RGB PNG (non interlaced, no ancillary chunks).
///
/// `rgb` must hold exactly width * height * 3 samples, row major. The file is
/// removed again when libpng fails, so no half written PNG is left behind.
/// Throws sltcd::WriteError on any failure.
void writeRgb(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgb, std::uint32_t width,
              std::uint32_t height);

} // namespace sltcd::png
