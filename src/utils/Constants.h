#pragma once

#include <cstdint>

namespace sltcd {

/// Format constants for the Second Life / Firestorm texture cache.
///
/// The layout below was verified against a real cache (Firestorm "texturecache"):
///   texture.entries : 44-byte header followed by N * 28-byte records
///   texture.cache   : N * 600-byte blocks with the first 600 bytes of each
///                     JPEG 2000 codestream; the remaining bytes live in the
///                     per-texture "<uuid>.texture" file.
///
/// The values are grouped in a struct so that a future format variant can be
/// described by a different configuration object (see plan section 5).
struct CacheFormatConfig {
    /// Size of the per-entry header that is stored in texture.cache. This is the
    /// documented (single allowed) "magic number" of the format.
    static constexpr std::uint32_t kTextureHeaderSize = 600;

    /// Serialized size of a single EntriesInfo record. Computed from the fields
    /// (float version + uint32 addressSize + 32 byte encoder id + uint32 count)
    /// rather than hard-coded, to avoid hidden magic numbers.
    static constexpr std::uint32_t kEntriesInfoSize = 4 + 4 + 32 + 4; // 44

    /// Serialized size of a single entry (16 byte UUID + 3 * int32). Used only
    /// in tests and assertions; parsing itself is always field-driven.
    static constexpr std::uint32_t kEntrySizeBytes = 16 + 4 + 4 + 4; // 28

    /// Sanity limit to reject obviously corrupt entry counts before allocating.
    static constexpr std::uint64_t kMaxReasonableEntries = 10'000'000ULL;

    /// Upper bound for a single texture body before allocating a buffer.
    static constexpr std::uint64_t kMaxBodySize = 1ULL << 32; // 4 GiB
};

} // namespace sltcd
