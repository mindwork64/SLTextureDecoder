#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "VersionInfo.h"
#include "utils/Constants.h"

TEST(VersionInfo, ReportsLinkedOpenJpegVersion) {
    const std::string version = sltcd::openjpegVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_EQ(version.substr(0, 2), "2.");
}

TEST(VersionInfo, ReportsLinkedLibpngVersion) {
    const std::string version = sltcd::libpngVersion();
    EXPECT_FALSE(version.empty());
    EXPECT_EQ(version.substr(0, 2), "1.");
}

TEST(VersionInfo, ReportsToolVersion) {
    EXPECT_EQ(sltcd::toolVersion(), "0.1.0");
}

TEST(CacheFormatConfig, MatchesRealCacheLayout) {
    // Values verified against a real Firestorm texturecache:
    //   texture.entries size = 44 + 196700 * 28 = 5'507'644 bytes.
    constexpr std::uint32_t kRealEntryCount = 196'700;
    constexpr std::uint32_t kRealEntriesFileSize = 5'507'644;

    EXPECT_EQ(sltcd::CacheFormatConfig::kTextureHeaderSize, 600u);
    EXPECT_EQ(sltcd::CacheFormatConfig::kEntriesInfoSize, 44u);
    EXPECT_EQ(sltcd::CacheFormatConfig::kEntrySizeBytes, 28u);

    EXPECT_EQ(sltcd::CacheFormatConfig::kEntriesInfoSize +
                  kRealEntryCount * sltcd::CacheFormatConfig::kEntrySizeBytes,
              kRealEntriesFileSize);
}
