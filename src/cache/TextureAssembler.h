#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "cache/CacheLayout.h"
#include "cache/TextureEntries.h"
#include "utils/UUID.h"

namespace sltcd::cache {

/// A JPEG 2000 codestream rebuilt from the two halves of the cache.
struct AssembledTexture {
    UUID id;
    std::uint32_t index = 0;
    std::vector<std::uint8_t> codestream;
    /// mImageSize of the record, i.e. the size the viewer announced (-1 when unknown).
    std::int64_t announcedImageSize = -1;
    /// True when the announced size equals the assembled size, which is the
    /// signature of a complete codestream - see docs/format-notes.md.
    bool complete = false;

    bool startsWithSoc() const noexcept;
    bool endsWithEoc() const noexcept;
};

/// Rebuilds codestreams from texture.cache and the <uuid>.texture body files.
class TextureAssembler {
public:
    explicit TextureAssembler(CacheLayout layout) : layout_(std::move(layout)) {}

    const CacheLayout& layout() const noexcept { return layout_; }

    /// Read the header block at `index`, read the body file of `entry` and join
    /// them into one codestream.
    ///
    /// Throws:
    ///   EntryNotFound - the record has no body (bodySize <= 0) or the body file
    ///                   does not exist
    ///   CacheTooSmall - texture.cache ends before the header block
    ///   SizeMismatch  - the body file size differs from the recorded bodySize
    ///   IoError       - any other I/O failure
    AssembledTexture assemble(const TextureEntry& entry, std::uint32_t index) const;

    /// Pure variant, used by tests and by callers that already have both halves.
    static AssembledTexture assembleBuffers(const TextureEntry& entry, std::uint32_t index,
                                            const std::vector<std::uint8_t>& headerBlock,
                                            const std::vector<std::uint8_t>& body);

private:
    CacheLayout layout_;
};

} // namespace sltcd::cache
