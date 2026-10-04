#include "utils/FileUtils.h"

#include "utils/Errors.h"

#include <fstream>
#include <string>
#include <system_error>

namespace sltcd::fileutils {
namespace {

/// Creates the parent directory of `path` when it is missing.
void ensureParentDirectory(const std::filesystem::path& path) {
    if (!path.has_parent_path()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        throw WriteError("cannot create directory '" + path.parent_path().string() + "': " + ec.message());
    }
}

/// Removes `path`, ignoring a failure: it is only used together with an error
/// that is already on its way out.
void removeQuietly(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

} // namespace

void removeFile(const std::filesystem::path& path) noexcept {
    removeQuietly(path);
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw IoError("cannot open file for reading: " + path.string());
    }

    const std::streamoff end = stream.tellg();
    if (end < 0) {
        throw IoError("cannot determine size of file: " + path.string());
    }
    stream.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    if (!data.empty() && !stream.read(reinterpret_cast<char*>(data.data()), end)) {
        throw IoError("failed to read file: " + path.string());
    }
    return data;
}

std::vector<std::uint8_t> readRange(const std::filesystem::path& path, std::uint64_t offset, std::size_t length) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw IoError("cannot open file for reading: " + path.string());
    }

    stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!stream) {
        throw IoError("cannot seek to offset " + std::to_string(offset) + " in " + path.string());
    }

    std::vector<std::uint8_t> data(length);
    if (length != 0 && !stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(length))) {
        throw IoError("short read at offset " + std::to_string(offset) + " in " + path.string());
    }
    return data;
}

void writeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    ensureParentDirectory(path);

    // Write to the sibling temporary file first: a reader (and the next run)
    // then either sees the old file or the new one, but never a half written
    // one, no matter when the process is stopped.
    const std::filesystem::path temporary = temporaryPath(path);

    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw WriteError("cannot open file for writing: " + temporary.string());
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    stream.flush();
    if (!stream) {
        stream.close();
        removeQuietly(temporary);
        throw WriteError("failed to write file: " + path.string());
    }
    stream.close();

    replaceFile(temporary, path);
}

std::filesystem::path temporaryPath(const std::filesystem::path& path) {
    std::filesystem::path temporary = path;
    temporary += ".part";
    return temporary;
}

void replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& target) {
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    if (!ec) {
        return;
    }

    // std::filesystem::rename() replaces an existing file, but not every file
    // system and implementation behaves that way, so retry via remove + rename -
    // and only for a regular file, never for a directory that happens to have
    // the name of the output file.
    std::error_code targetEc;
    const bool replaceable = std::filesystem::is_regular_file(target, targetEc);
    if (!replaceable) {
        removeQuietly(temporary);
        throw WriteError("cannot move " + temporary.string() + " to " + target.string() + ": " + ec.message());
    }

    removeQuietly(target);
    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        removeQuietly(temporary);
        throw WriteError("cannot move " + temporary.string() + " to " + target.string() + ": " + ec.message());
    }
}

bool exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::uintmax_t size(const std::filesystem::path& path) {
    std::error_code ec;
    const auto value = std::filesystem::file_size(path, ec);
    return ec ? 0 : value;
}

} // namespace sltcd::fileutils
