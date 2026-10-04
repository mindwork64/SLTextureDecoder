#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "cache/CacheLayout.h"
#include "cache/TextureAssembler.h"
#include "cache/TextureEntries.h"
#include "jpeg2000/DecodedImage.h"
#include "utils/UUID.h"

namespace sltcd::cache {

/// One record of the cache, decoded.
struct DecodedTexture {
    UUID id;
    std::uint32_t index = 0;
    /// True when the cache held the whole codestream (TextureEntry::isComplete).
    bool complete = false;
    /// Samples as they came out of the decoder; component handling happens
    /// separately (sltcd::jpeg2000::toRgba).
    jpeg2000::DecodedImage image;
    /// Messages OpenJPEG reported while decoding. Informational notices land
    /// here as well, so `complete` (not this string) tells a full decode from a
    /// salvaged one.
    std::string diagnostics;
    /// The assembled codestream, filled only when decode() was asked for it
    /// (`keepCodestream`), so `--keep-j2k` can store it.
    std::vector<std::uint8_t> codestream;
};

/// Iterates a texture cache directory: reads texture.entries and rebuilds and
/// decodes the codestream of every record that has a body file.
class TextureCacheReader {
public:
    /// Open `directory`. Throws CacheNotFound when it does not look like a
    /// cache, InvalidFormat when texture.entries is broken, IoError otherwise.
    static TextureCacheReader open(const std::filesystem::path& directory);

    const std::filesystem::path& root() const noexcept { return layout_.root(); }
    const CacheLayout& layout() const noexcept { return layout_; }
    const EntriesInfo& info() const noexcept { return entries_.info(); }
    const TextureEntries& entries() const noexcept { return entries_; }

    /// Record indices, in record order, whose slot holds a body file. With
    /// `completeOnly` the truncated records are left out.
    std::vector<std::uint32_t> decodableIndices(bool completeOnly = false) const;

    /// Assemble and decode the record at `index`. Complete records are decoded
    /// strictly, truncated ones leniently (best effort).
    ///
    /// Throws EntryNotFound (bad index, no body, missing body file),
    /// CacheTooSmall, SizeMismatch, DecodeError, IoError.
    /// With `keepCodestream` the assembled codestream is kept as well
    /// (DecodedTexture::codestream), which is what `--keep-j2k` stores.
    DecodedTexture decode(std::uint32_t index, bool keepCodestream = false) const;

private:
    TextureCacheReader(CacheLayout layout, TextureEntries entries);

    CacheLayout layout_;
    TextureEntries entries_;
    TextureAssembler assembler_;
};

} // namespace sltcd::cache
