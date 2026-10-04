#include "cache/TextureAssembler.h"

#include <filesystem>
#include <string>

#include "utils/Constants.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace sltcd::cache {
namespace {

constexpr std::size_t kHeaderBlockSize = CacheFormatConfig::kTextureHeaderSize;

/// Rejects a record whose announced body cannot belong to a texture of this
/// format. Checked before any file is read, so a corrupt texture.entries can
/// neither make the tool load gigabytes nor copy them around.
void checkBodySize(const TextureEntry& entry, std::uint32_t index) {
    if (static_cast<std::uint64_t>(entry.bodySize) > CacheFormatConfig::kMaxBodySize) {
        throw InvalidFormat("record " + std::to_string(index) + " announces a body of " +
                            std::to_string(entry.bodySize) + " bytes, which is not plausible for this format");
    }
}

} // namespace

bool AssembledTexture::startsWithSoc() const noexcept {
    return codestream.size() >= 2 && codestream[0] == 0xFF && codestream[1] == 0x4F;
}

bool AssembledTexture::endsWithEoc() const noexcept {
    return codestream.size() >= 2 && codestream[codestream.size() - 2] == 0xFF &&
           codestream[codestream.size() - 1] == 0xD9;
}

AssembledTexture TextureAssembler::assembleBuffers(const TextureEntry& entry, std::uint32_t index,
                                                   const std::vector<std::uint8_t>& headerBlock,
                                                   const std::vector<std::uint8_t>& body) {
    checkBodySize(entry, index);

    if (headerBlock.size() != kHeaderBlockSize) {
        throw CacheTooSmall("header block of entry " + std::to_string(index) + " is " +
                            std::to_string(headerBlock.size()) + " bytes, expected " +
                            std::to_string(kHeaderBlockSize));
    }

    AssembledTexture result;
    result.id = entry.id;
    result.index = index;
    result.announcedImageSize = entry.imageSize;
    result.codestream.reserve(kHeaderBlockSize + body.size());
    result.codestream.insert(result.codestream.end(), headerBlock.begin(), headerBlock.end());
    result.codestream.insert(result.codestream.end(), body.begin(), body.end());
    result.complete = entry.imageSize > 0 && entry.imageSize == static_cast<std::int64_t>(result.codestream.size());
    return result;
}

AssembledTexture TextureAssembler::assemble(const TextureEntry& entry, std::uint32_t index) const {
    if (!entry.hasBody()) {
        throw EntryNotFound("entry " + std::to_string(index) + " (" + entry.id.toString() + ") has no body");
    }
    checkBodySize(entry, index); // before the body file is read into memory

    const std::filesystem::path headerPath = layout_.headerCacheFile();
    const std::uint64_t offset = CacheLayout::headerBlockOffset(index);
    const std::uint64_t headerFileSize = fileutils::size(headerPath);
    if (headerFileSize < offset + kHeaderBlockSize) {
        throw CacheTooSmall("texture.cache is too small for entry " + std::to_string(index) + ": " +
                            std::to_string(headerFileSize) + " bytes, need " +
                            std::to_string(offset + kHeaderBlockSize));
    }

    const std::vector<std::uint8_t> headerBlock = fileutils::readRange(headerPath, offset, kHeaderBlockSize);

    const std::filesystem::path bodyPath = layout_.bodyFile(entry.id);
    if (!fileutils::exists(bodyPath)) {
        throw EntryNotFound("body file not found: " + bodyPath.string());
    }

    // Compare the sizes before reading: a file that does not match the record
    // must never be pulled into memory, whatever its size is.
    const std::uintmax_t bodyFileSize = fileutils::size(bodyPath);
    if (bodyFileSize != static_cast<std::uintmax_t>(entry.bodySize)) {
        throw SizeMismatch("body file " + bodyPath.string() + " holds " + std::to_string(bodyFileSize) +
                           " bytes but the record announces " + std::to_string(entry.bodySize));
    }

    const std::vector<std::uint8_t> body = fileutils::readFile(bodyPath);
    return assembleBuffers(entry, index, headerBlock, body);
}

} // namespace sltcd::cache
