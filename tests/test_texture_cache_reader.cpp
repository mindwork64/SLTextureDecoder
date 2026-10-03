#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "TestCacheFixture.h"
#include "cache/TextureCacheReader.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace {

bool contains(const std::vector<std::uint32_t>& indices, std::uint32_t index) {
    return std::find(indices.begin(), indices.end(), index) != indices.end();
}

} // namespace

TEST(TextureCacheReader, RejectsAPathThatIsNotACache) {
    EXPECT_THROW(sltcd::cache::TextureCacheReader::open("D:/sltcd/does-not-exist"), sltcd::CacheNotFound);

    const std::filesystem::path empty = std::filesystem::temp_directory_path() / "sltcd_texture_cache_reader_empty";
    std::filesystem::remove_all(empty);
    std::filesystem::create_directories(empty);
    EXPECT_THROW(sltcd::cache::TextureCacheReader::open(empty), sltcd::CacheNotFound);
    std::filesystem::remove_all(empty);
}

TEST(TextureCacheReader, ListsTheRecordsThatHaveABody) {
    const sltcd::test::CacheFixture fixture("sltcd_texture_cache_reader");
    const sltcd::cache::TextureCacheReader reader = sltcd::cache::TextureCacheReader::open(fixture.root());

    EXPECT_EQ(reader.entries().size(), 395U);

    // Record 0 is a free slot, so its orphan body file is ignored.
    const std::vector<std::uint32_t> all = reader.decodableIndices();
    EXPECT_FALSE(contains(all, 0U));
    EXPECT_TRUE(contains(all, 100U));
    EXPECT_TRUE(contains(all, 394U));
    EXPECT_TRUE(std::is_sorted(all.begin(), all.end())); // record order

    // Record 100 holds only a prefix of its codestream.
    const std::vector<std::uint32_t> complete = reader.decodableIndices(/*completeOnly=*/true);
    EXPECT_FALSE(contains(complete, 100U));
    EXPECT_TRUE(contains(complete, 394U));
}

TEST(TextureCacheReader, DecodesACompleteRecordByteForByte) {
    const sltcd::test::CacheFixture fixture("sltcd_texture_cache_reader");
    const sltcd::cache::TextureCacheReader reader = sltcd::cache::TextureCacheReader::open(fixture.root());

    const sltcd::cache::DecodedTexture texture = reader.decode(394);
    EXPECT_EQ(texture.id.toString(), sltcd::test::kUuidRecord394);
    // `complete` comes from the record, not from the diagnostics: OpenJPEG also
    // reports "main header read" style notices on a perfectly clean decode.
    EXPECT_TRUE(texture.complete);
    EXPECT_EQ(texture.image.width, 16U);
    EXPECT_EQ(texture.image.height, 256U);
    EXPECT_EQ(texture.image.components, 4U);

    const std::vector<std::uint8_t> reference = sltcd::fileutils::readFile(sltcd::test::dataFile("reference_394.rgba"));
    EXPECT_EQ(texture.image.pixels, reference);
}

TEST(TextureCacheReader, DecodesATruncatedRecordLeniently) {
    const sltcd::test::CacheFixture fixture("sltcd_texture_cache_reader");
    const sltcd::cache::TextureCacheReader reader = sltcd::cache::TextureCacheReader::open(fixture.root());

    const sltcd::cache::DecodedTexture texture = reader.decode(100);
    EXPECT_EQ(texture.id.toString(), sltcd::test::kUuidRecord100);
    EXPECT_FALSE(texture.complete);
    EXPECT_FALSE(texture.diagnostics.empty());
    EXPECT_EQ(texture.image.width, 512U);
    EXPECT_EQ(texture.image.components, 3U);
}

TEST(TextureCacheReader, RejectsAnIndexOutsideTheRecordArray) {
    const sltcd::test::CacheFixture fixture("sltcd_texture_cache_reader");
    const sltcd::cache::TextureCacheReader reader = sltcd::cache::TextureCacheReader::open(fixture.root());

    EXPECT_THROW(reader.decode(395), sltcd::EntryNotFound);
}
