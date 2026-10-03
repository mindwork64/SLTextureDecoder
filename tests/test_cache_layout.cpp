#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include "cache/CacheLayout.h"
#include "utils/Constants.h"
#include "utils/UUID.h"

namespace {

sltcd::UUID uuid(const char* text) {
    return sltcd::UUID::parse(text);
}

} // namespace

TEST(CacheLayout, ShardNameIsTheFirstHexDigitOfTheUuid) {
    // Verified against a real cache: e569711a-... lives in "e", 87d1503c-... in "8".
    EXPECT_EQ(sltcd::cache::CacheLayout::shardName(uuid("e569711a-27c2-aad4-9246-0c910239a179")), 'e');
    EXPECT_EQ(sltcd::cache::CacheLayout::shardName(uuid("87d1503c-9713-3228-9672-0a019a956b5e")), '8');
    EXPECT_EQ(sltcd::cache::CacheLayout::shardName(uuid("00000000-0000-0000-0000-000000000000")), '0');
    EXPECT_EQ(sltcd::cache::CacheLayout::shardName(uuid("ffffffff-ffff-ffff-ffff-ffffffffffff")), 'f');
}

TEST(CacheLayout, WellKnownFileNames) {
    const sltcd::cache::CacheLayout layout(std::filesystem::path("C:/cache"));

    EXPECT_EQ(layout.entriesFile(), std::filesystem::path("C:/cache/texture.entries"));
    EXPECT_EQ(layout.headerCacheFile(), std::filesystem::path("C:/cache/texture.cache"));
    EXPECT_EQ(layout.fastCacheFile(), std::filesystem::path("C:/cache/FastCache.cache"));
}

TEST(CacheLayout, BodyFileUsesShardDirectory) {
    const sltcd::cache::CacheLayout layout(std::filesystem::path("C:/cache"));

    EXPECT_EQ(layout.bodyFile(uuid("87d1503c-9713-3228-9672-0a019a956b5e")),
              std::filesystem::path("C:/cache/8/87d1503c-9713-3228-9672-0a019a956b5e.texture"));
    EXPECT_EQ(layout.bodyFile(uuid("e569711a-27c2-aad4-9246-0c910239a179")),
              std::filesystem::path("C:/cache/e/e569711a-27c2-aad4-9246-0c910239a179.texture"));
}

TEST(CacheLayout, HeaderBlockOffsetIsSixHundredBytesPerRecord) {
    EXPECT_EQ(sltcd::cache::CacheLayout::headerBlockOffset(0), 0ULL);
    EXPECT_EQ(sltcd::cache::CacheLayout::headerBlockOffset(1), 600ULL);
    EXPECT_EQ(sltcd::cache::CacheLayout::headerBlockOffset(394), 236'400ULL);
    EXPECT_EQ(sltcd::cache::CacheLayout::headerBlockOffset(186'869), 112'121'400ULL);

    // A real texture.cache holds exactly entriesCount * 600 bytes.
    EXPECT_EQ(sltcd::cache::CacheLayout::headerBlockOffset(196'700), 118'020'000ULL);
    EXPECT_EQ(sltcd::CacheFormatConfig::kTextureHeaderSize, 600U);
}
