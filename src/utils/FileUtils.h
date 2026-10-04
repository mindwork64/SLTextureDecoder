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
///
/// The file appears atomically: the data goes to the temporary sibling
/// temporaryPath(path) first and is renamed over `path` afterwards, so an
/// interruption (Ctrl+C, power loss) leaves either the old file or the new one,
/// never a half written file. Throws sltcd::WriteError on failure; a failed
/// write does not touch `path`.
void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data);

/// Path of the temporary file writeFile() and sltcd::png use for `path`, i.e.
/// `<path>.part`. It lives in the same directory as `path` so that the final
/// rename stays on one file system and is atomic.
std::filesystem::path temporaryPath(const std::filesystem::path& path);

/// Rename `temporary` onto `target`, replacing an existing target.
///
/// Throws sltcd::WriteError when the rename fails; `target` is then left
/// untouched and `temporary` is removed, so a failed call never leaves a half
/// written file behind.
void replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& target);

bool exists(const std::filesystem::path& path);

/// Delete `path` when it is there. Never throws: it is used to drop the
/// temporary file of a failed write, where a second error would only hide the
/// first one.
void removeFile(const std::filesystem::path& path) noexcept;

/// File size in bytes; 0 when the file does not exist.
std::uintmax_t size(const std::filesystem::path& path);

} // namespace sltcd::fileutils
