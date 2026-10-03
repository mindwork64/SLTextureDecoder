#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "utils/Constants.h"
#include "utils/UUID.h"

namespace sltcd {

/// The 44 byte header of a texture.entries file.
///
/// Mirrors LLTextureCache::EntriesInfo:
///     F32 mVersion; U32 mAdressSize; char mEncoderVersion[32]; S32 mEntries;
struct EntriesInfo {
    float version = 0.0F;
    std::uint32_t addressSize = 0;
    std::string encoderVersion; ///< NUL padded in the file, trimmed on parse
    std::uint32_t entriesCount = 0;
};

/// One 28 byte texture.entries record.
///
/// Mirrors LLTextureCache::Entry:
///     LLUUID mID; S32 mImageSize; S32 mBodySize; U32 mTime;
struct TextureEntry {
    UUID id;
    /// "Total size of image if known", -1 while the record is brand new.
    std::int32_t imageSize = 0;
    /// Size of the <uuid>.texture body file. <= 0 means the slot is free.
    std::int32_t bodySize = 0;
    /// Seconds since 01.01.1970.
    std::uint32_t timestamp = 0;

    /// A slot is usable only when a body file was written for it. Verified on a
    /// real cache: the number of records with bodySize > 0 equals the number of
    /// .texture files on disk exactly.
    bool hasBody() const noexcept { return bodySize > 0; }

    /// Body plus the 600 byte header block reproduce the whole codestream.
    bool isComplete() const noexcept;

    /// Observed for ~58% of a real cache: the header block and the body do not
    /// form a complete codestream (see docs/format-notes.md). Such records hold
    /// only a prefix of the image data.
    bool isPartial() const noexcept;

    /// True when the record itself does not know the image size.
    bool hasUnknownImageSize() const noexcept { return imageSize <= 0; }

    /// Total size the codestream would have when header and body are joined.
    std::int64_t assembledSize() const noexcept;
};

/// Parsed contents of a texture.entries file.
class TextureEntries {
public:
    /// Parse only the 44 byte header. Throws InvalidFormat for short input.
    static EntriesInfo parseInfo(const std::vector<std::uint8_t>& bytes);

    /// Parse a complete file. Throws InvalidFormat when the header is
    /// implausible or when the file is shorter than the header announces.
    static TextureEntries parse(const std::vector<std::uint8_t>& bytes);

    static TextureEntries load(const std::filesystem::path& path);

    const EntriesInfo& info() const noexcept { return info_; }
    const std::vector<TextureEntry>& entries() const noexcept { return entries_; }
    std::size_t size() const noexcept { return entries_.size(); }
    bool empty() const noexcept { return entries_.empty(); }

    /// Throws std::out_of_range when `index` is not valid.
    const TextureEntry& at(std::size_t index) const;

    /// Position of the record for `id`. The array is unordered, so this is a
    /// linear scan; callers that need many lookups should build their own map.
    std::optional<std::size_t> indexOf(const UUID& id) const;

    /// Serialized size of a file that describes `entriesCount` records.
    static std::uint64_t expectedFileSize(std::uint32_t entriesCount) noexcept;

private:
    EntriesInfo info_;
    std::vector<TextureEntry> entries_;
};

} // namespace sltcd
