#include "cache/TextureCacheReader.h"

#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "jpeg2000/Jpeg2000Decoder.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace sltcd::cache {

TextureCacheReader::TextureCacheReader(CacheLayout layout, TextureEntries entries)
    : layout_(std::move(layout)), entries_(std::move(entries)), assembler_(layout_) {}

TextureCacheReader TextureCacheReader::open(const std::filesystem::path& directory) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        throw CacheNotFound("not a directory: " + directory.string());
    }

    CacheLayout layout(directory);
    if (!fileutils::exists(layout.entriesFile())) {
        throw CacheNotFound("no texture.entries in " + directory.string());
    }
    if (!fileutils::exists(layout.headerCacheFile())) {
        throw CacheNotFound("no texture.cache in " + directory.string());
    }

    TextureEntries entries = TextureEntries::load(layout.entriesFile());
    return TextureCacheReader(std::move(layout), std::move(entries));
}

std::vector<std::uint32_t> TextureCacheReader::decodableIndices(bool completeOnly) const {
    std::vector<std::uint32_t> indices;
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const TextureEntry& entry = entries_.at(i);
        if (!entry.hasBody()) {
            continue;
        }
        if (completeOnly && !entry.isComplete()) {
            continue;
        }
        indices.push_back(static_cast<std::uint32_t>(i));
    }
    return indices;
}

DecodedTexture TextureCacheReader::decode(std::uint32_t index) const {
    if (index >= entries_.size()) {
        throw EntryNotFound("record " + std::to_string(index) + " is out of range, the cache holds " +
                            std::to_string(entries_.size()) + " records");
    }

    const TextureEntry& entry = entries_.at(index);
    const AssembledTexture assembled = assembler_.assemble(entry, index);

    DecodedTexture result;
    result.id = entry.id;
    result.index = index;
    result.complete = assembled.complete;

    // A complete codestream is decoded strictly, so any complaint is a real
    // defect. A truncated record is retried leniently, which returns the detail
    // that survived in the cache (docs/format-notes.md).
    const std::optional<jpeg2000::DecodedImage> image = jpeg2000::Jpeg2000Decoder::tryDecode(
        assembled.codestream, jpeg2000::DecodeOptions{assembled.complete}, &result.diagnostics);
    if (!image.has_value()) {
        throw DecodeError("cannot decode texture " + entry.id.toString() + " (record " + std::to_string(index) + ")" +
                          (result.diagnostics.empty() ? std::string() : ": " + result.diagnostics));
    }

    result.image = *image;
    return result;
}

} // namespace sltcd::cache
