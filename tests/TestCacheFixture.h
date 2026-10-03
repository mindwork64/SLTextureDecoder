#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "cache/CacheLayout.h"
#include "utils/FileUtils.h"
#include "utils/UUID.h"

namespace sltcd::test {

/// A file of the frozen slice in tests/data.
inline std::filesystem::path dataFile(const std::string& name) {
    return std::filesystem::path(SLTCD_TEST_DATA_DIR) / name;
}

inline constexpr const char* kUuidRecord100 = "87d1503c-9713-3228-9672-0a019a956b5e";
inline constexpr const char* kUuidRecord394 = "6ffba690-3dd3-3e31-0913-df429d3204c6";

/// A throw-away cache directory that mirrors tests/data: the 395 records of the
/// frozen slice, a texture.cache that carries the real blocks of records 100 and
/// 394 (all others stay zeroed) and the body files the slice ships.
///
/// Record 0 is a free slot that nevertheless has a stale body file on disk, so
/// it also covers "orphan file of a reusable slot".
class CacheFixture {
public:
    explicit CacheFixture(const std::string& name)
        : root_(std::filesystem::temp_directory_path() / name), layout_(root_) {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
        std::filesystem::copy_file(dataFile("entries_mini.bin"), layout_.entriesFile());

        std::vector<std::uint8_t> cache(395 * 600, 0);
        putBlock(cache, 100, "block_100.bin");
        putBlock(cache, 394, "block_394.bin");
        fileutils::writeFile(layout_.headerCacheFile(), cache);

        copyBody("e569711a-27c2-aad4-9246-0c910239a179", "body_0.texture");
        copyBody(kUuidRecord100, "body_100.texture");
        copyBody(kUuidRecord394, "body_394.texture");
    }

    ~CacheFixture() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    CacheFixture(const CacheFixture&) = delete;
    CacheFixture& operator=(const CacheFixture&) = delete;

    const std::filesystem::path& root() const noexcept { return root_; }

private:
    void putBlock(std::vector<std::uint8_t>& cache, std::size_t index, const std::string& file) {
        const std::vector<std::uint8_t> block = fileutils::readFile(dataFile(file));
        std::copy(block.begin(), block.end(), cache.begin() + static_cast<std::ptrdiff_t>(index * 600));
    }

    void copyBody(const std::string& uuid, const std::string& file) {
        const std::filesystem::path target = layout_.bodyFile(UUID::parse(uuid));
        std::filesystem::create_directories(target.parent_path());
        std::filesystem::copy_file(dataFile(file), target);
    }

    std::filesystem::path root_;
    cache::CacheLayout layout_;
};

} // namespace sltcd::test
