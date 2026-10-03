#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "jpeg2000/DecodedImage.h"

namespace sltcd::jpeg2000 {

/// Options forwarded to OpenJPEG.
struct DecodeOptions {
    /// Strict mode rejects codestream errors. The default matches the
    /// standalone opj_decompress utility. Relaxing it lets the decoder salvage
    /// what is present in a truncated codestream (see docs/format-notes.md).
    bool strict = true;
};

/// Decodes one in-memory JPEG 2000 codestream (raw J2K, not a JP2 container).
class Jpeg2000Decoder {
public:
    /// Decode `codestream`, throwing sltcd::DecodeError on any failure. The
    /// thrown message contains the diagnostics reported by OpenJPEG.
    static DecodedImage decode(const std::vector<std::uint8_t>& codestream, const DecodeOptions& options = {});

    /// Same, but returns std::nullopt instead of throwing. Batch conversions
    /// use this so that one broken texture does not abort the run. When
    /// `diagnostics` is not null it receives the OpenJPEG messages, if any.
    static std::optional<DecodedImage> tryDecode(const std::vector<std::uint8_t>& codestream,
                                                 const DecodeOptions& options = {}, std::string* diagnostics = nullptr);
};

} // namespace sltcd::jpeg2000
