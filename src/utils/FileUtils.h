#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace sltcd::fileutils {

/// Read a whole file into memory. Throws sltcd::IoError on failure.
std::vector<std::uint8_t> readFile(const std::filesystem::path& path);

/// Read `length` bytes starting at `offset`.
///
/// Reads only the requested range and therefore never materialises a whole
/// multi-gigabyte cache file. Throws sltcd::IoError on failure.
std::vector<std::uint8_t> readRange(const std::filesystem::path& path, std::uint64_t offset, std::size_t length);

/// Write `data` to `path`, truncating any existing file.
/// Throws sltcd::WriteError on failure.
void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data);

bool exists(const std::filesystem::path& path);

/// File size in bytes; 0 when the file does not exist.
std::uintmax_t size(const std::filesystem::path& path);

} // namespace sltcd::fileutils
