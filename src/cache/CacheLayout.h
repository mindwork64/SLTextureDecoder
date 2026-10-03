#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "utils/UUID.h"

namespace sltcd::cache {

/// Maps the logical cache content onto the files of a Second Life / Firestorm
/// texture cache directory:
///
///   <root>/texture.entries          unordered array of 28 byte records
///   <root>/texture.cache            first 600 bytes of every texture, in the
///                                   same order as the records above
///   <root>/FastCache.cache          16x16 RGBA previews (1040 bytes / entry)
///   <root>/<0..f>/<uuid>.texture    remainder of the codestream of one texture
///
/// The layout was verified against a real cache and against the Firestorm
/// sources (indra/newview/lltexturecache.cpp); see docs/format-notes.md.
class CacheLayout {
public:
    explicit CacheLayout(std::filesystem::path root);

    const std::filesystem::path& root() const noexcept { return root_; }

    std::filesystem::path entriesFile() const;
    std::filesystem::path headerCacheFile() const;
    std::filesystem::path fastCacheFile() const;

    /// Body file of one texture: <root>/<shard>/<uuid>.texture
    std::filesystem::path bodyFile(const UUID& id) const;

    /// Byte offset of the 600 byte header block of `index` in texture.cache.
    static std::uint64_t headerBlockOffset(std::uint32_t index) noexcept;

    /// Shard directory name of `id`: the first hexadecimal digit of the
    /// canonical textual form, i.e. the high nibble of the first byte.
    static char shardName(const UUID& id) noexcept;

private:
    std::filesystem::path root_;
};

} // namespace sltcd::cache
