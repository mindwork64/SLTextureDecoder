#include "cache/CacheLayout.h"

#include <utility>

#include "utils/Constants.h"

namespace sltcd::cache {
namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

} // namespace

CacheLayout::CacheLayout(std::filesystem::path root) : root_(std::move(root)) {}

std::filesystem::path CacheLayout::entriesFile() const {
    return root_ / "texture.entries";
}

std::filesystem::path CacheLayout::headerCacheFile() const {
    return root_ / "texture.cache";
}

std::filesystem::path CacheLayout::fastCacheFile() const {
    return root_ / "FastCache.cache";
}

std::filesystem::path CacheLayout::bodyFile(const UUID& id) const {
    return root_ / std::string(1, shardName(id)) / (id.toString() + ".texture");
}

std::uint64_t CacheLayout::headerBlockOffset(std::uint32_t index) noexcept {
    return static_cast<std::uint64_t>(index) * CacheFormatConfig::kTextureHeaderSize;
}

char CacheLayout::shardName(const UUID& id) noexcept {
    return kHexDigits[id.bytes()[0] >> 4];
}

} // namespace sltcd::cache
