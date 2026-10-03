#include "cache/TextureEntries.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace sltcd {
namespace {

/// Size of the NUL padded encoder identification inside the header.
constexpr std::size_t kEncoderVersionFieldSize = 32;

/// Field offsets inside a 28 byte record.
constexpr std::size_t kEntryIdOffset = 0;
constexpr std::size_t kEntryImageSizeOffset = 16;
constexpr std::size_t kEntryBodySizeOffset = 20;
constexpr std::size_t kEntryTimestampOffset = 24;

static_assert(4 + 4 + kEncoderVersionFieldSize + 4 == CacheFormatConfig::kEntriesInfoSize,
              "header layout no longer matches CacheFormatConfig::kEntriesInfoSize");
static_assert(kEntryTimestampOffset + 4 == CacheFormatConfig::kEntrySizeBytes,
              "record layout no longer matches CacheFormatConfig::kEntrySizeBytes");

/// The cache is little endian regardless of the host (it is written with a
/// plain memcpy by the viewer), so read the bytes explicitly.
std::uint32_t readU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t readI32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(readU32(p));
}

float readF32(const std::uint8_t* p) {
    const std::uint32_t bits = readU32(p);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string readFixedString(const std::uint8_t* p, std::size_t maxLength) {
    std::size_t length = 0;
    while (length < maxLength && p[length] != 0) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(p), length);
}

} // namespace

EntriesInfo TextureEntries::parseInfo(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < CacheFormatConfig::kEntriesInfoSize) {
        throw InvalidFormat("texture.entries is too small (" + std::to_string(bytes.size()) + " bytes, at least " +
                            std::to_string(CacheFormatConfig::kEntriesInfoSize) + " are required for the header)");
    }

    EntriesInfo info;
    info.version = readF32(bytes.data());
    info.addressSize = readU32(bytes.data() + 4);
    info.encoderVersion = readFixedString(bytes.data() + 8, kEncoderVersionFieldSize);
    info.entriesCount = readU32(bytes.data() + 40);

    if (info.entriesCount > CacheFormatConfig::kMaxReasonableEntries) {
        throw InvalidFormat("texture.entries announces " + std::to_string(info.entriesCount) +
                            " records, which is not plausible");
    }
    return info;
}

TextureEntries TextureEntries::parse(const std::vector<std::uint8_t>& bytes) {
    TextureEntries result;
    result.info_ = parseInfo(bytes);

    const std::uint64_t expected = expectedFileSize(result.info_.entriesCount);
    if (bytes.size() < expected) {
        throw InvalidFormat("texture.entries is truncated: " + std::to_string(bytes.size()) + " bytes present but " +
                            std::to_string(expected) + " are required for " +
                            std::to_string(result.info_.entriesCount) + " records");
    }

    result.entries_.reserve(result.info_.entriesCount);
    const std::uint8_t* cursor = bytes.data() + CacheFormatConfig::kEntriesInfoSize;
    for (std::uint32_t i = 0; i < result.info_.entriesCount; ++i) {
        TextureEntry entry;
        UUID::Bytes raw{};
        std::memcpy(raw.data(), cursor + kEntryIdOffset, raw.size());
        entry.id = UUID::fromBytes(raw);
        entry.imageSize = readI32(cursor + kEntryImageSizeOffset);
        entry.bodySize = readI32(cursor + kEntryBodySizeOffset);
        entry.timestamp = readU32(cursor + kEntryTimestampOffset);
        result.entries_.push_back(entry);
        cursor += CacheFormatConfig::kEntrySizeBytes;
    }
    return result;
}

TextureEntries TextureEntries::load(const std::filesystem::path& path) {
    return parse(fileutils::readFile(path));
}

const TextureEntry& TextureEntries::at(std::size_t index) const {
    return entries_.at(index);
}

std::optional<std::size_t> TextureEntries::indexOf(const UUID& id) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

std::uint64_t TextureEntries::expectedFileSize(std::uint32_t entriesCount) noexcept {
    return CacheFormatConfig::kEntriesInfoSize +
           static_cast<std::uint64_t>(entriesCount) * CacheFormatConfig::kEntrySizeBytes;
}

bool TextureEntry::isComplete() const noexcept {
    return hasBody() && imageSize == bodySize + static_cast<std::int32_t>(CacheFormatConfig::kTextureHeaderSize);
}

bool TextureEntry::isPartial() const noexcept {
    return hasBody() && !isComplete();
}

std::int64_t TextureEntry::assembledSize() const noexcept {
    return static_cast<std::int64_t>(CacheFormatConfig::kTextureHeaderSize) + bodySize;
}

} // namespace sltcd
