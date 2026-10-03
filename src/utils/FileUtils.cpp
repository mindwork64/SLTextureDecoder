#include "utils/FileUtils.h"

#include "utils/Errors.h"

#include <fstream>
#include <string>

namespace sltcd::fileutils {

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
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw WriteError("cannot create directory '" + path.parent_path().string() + "': " + ec.message());
        }
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw WriteError("cannot open file for writing: " + path.string());
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    stream.flush();
    if (!stream) {
        throw WriteError("failed to write file: " + path.string());
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
