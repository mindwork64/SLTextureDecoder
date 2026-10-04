#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "cache/CacheLayout.h"
#include "cache/TextureAssembler.h"
#include "cache/TextureEntries.h"
#include "utils/Constants.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace {

std::filesystem::path dataFile(const std::string& name) {
    return std::filesystem::path(SLTCD_TEST_DATA_DIR) / name;
}

constexpr const char* kUuidRecord394 = "6ffba690-3dd3-3e31-0913-df429d3204c6";

sltcd::TextureEntries miniEntries() {
    return sltcd::TextureEntries::load(dataFile("entries_mini.bin"));
}

/// Builds a throw-away cache directory that holds the frozen block/body pair of
/// record 394 at its real offsets, so the file based path can be exercised.
class TextureAssemblerFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() / "sltcd_texture_assembler_test";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / "6");

        layout_ = std::make_unique<sltcd::cache::CacheLayout>(root_);

        // texture.cache: 395 blocks, with the real block of record 394 in place.
        std::vector<std::uint8_t> cache(395 * 600, 0);
        const std::vector<std::uint8_t> block = sltcd::fileutils::readFile(dataFile("block_394.bin"));
        std::copy(block.begin(), block.end(), cache.begin() + 394 * 600);
        sltcd::fileutils::writeFile(layout_->headerCacheFile(), cache);

        std::filesystem::copy_file(dataFile("body_394.texture"), layout_->bodyFile(sltcd::UUID::parse(kUuidRecord394)));
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    std::filesystem::path root_;
    std::unique_ptr<sltcd::cache::CacheLayout> layout_;
};

} // namespace

TEST(TextureAssembler, JoinsHeaderBlockAndBodyOfACompleteRecord) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::TextureEntry& entry = entries.at(394);

    const sltcd::cache::AssembledTexture assembled = sltcd::cache::TextureAssembler::assembleBuffers(
        entry, 394, sltcd::fileutils::readFile(dataFile("block_394.bin")),
        sltcd::fileutils::readFile(dataFile("body_394.texture")));

    EXPECT_EQ(assembled.id.toString(), kUuidRecord394);
    EXPECT_EQ(assembled.index, 394U);
    ASSERT_EQ(assembled.codestream.size(), 4101U);
    EXPECT_EQ(assembled.announcedImageSize, 4101);
    EXPECT_TRUE(assembled.complete);
    EXPECT_TRUE(assembled.startsWithSoc());
    EXPECT_TRUE(assembled.endsWithEoc());

    // The header block must be copied verbatim in front of the body.
    const std::vector<std::uint8_t> block = sltcd::fileutils::readFile(dataFile("block_394.bin"));
    EXPECT_TRUE(std::equal(block.begin(), block.end(), assembled.codestream.begin()));
}

TEST(TextureAssembler, JoinsAPartialRecordWithoutFlaggingItComplete) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::TextureEntry& entry = entries.at(100);

    const sltcd::cache::AssembledTexture assembled = sltcd::cache::TextureAssembler::assembleBuffers(
        entry, 100, sltcd::fileutils::readFile(dataFile("block_100.bin")),
        sltcd::fileutils::readFile(dataFile("body_100.texture")));

    ASSERT_EQ(assembled.codestream.size(), 1536U);
    EXPECT_EQ(assembled.codestream.size(), static_cast<std::size_t>(entry.assembledSize()));

    // The cached prefix of a partial record neither reaches the announced size
    // nor carries the end-of-codestream marker.
    EXPECT_EQ(assembled.announcedImageSize, 1537);
    EXPECT_FALSE(assembled.complete);
    EXPECT_TRUE(assembled.startsWithSoc());
    EXPECT_FALSE(assembled.endsWithEoc());
}

TEST(TextureAssembler, RejectsHeaderBlockWithWrongSize) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::TextureEntry& entry = entries.at(394);

    const std::vector<std::uint8_t> shortBlock(599, 0);
    EXPECT_THROW(sltcd::cache::TextureAssembler::assembleBuffers(entry, 394, shortBlock, {}), sltcd::CacheTooSmall);
    EXPECT_THROW(sltcd::cache::TextureAssembler::assembleBuffers(entry, 394, std::vector<std::uint8_t>(601, 0), {}),
                 sltcd::CacheTooSmall);
}

TEST(TextureAssembler, EmptyMarkerHelpersAreSafe) {
    const sltcd::cache::AssembledTexture empty;
    EXPECT_FALSE(empty.startsWithSoc());
    EXPECT_FALSE(empty.endsWithEoc());
}

TEST_F(TextureAssemblerFileTest, ReadsTheStreamFromTheCacheDirectory) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::cache::TextureAssembler assembler(*layout_);

    const sltcd::cache::AssembledTexture assembled = assembler.assemble(entries.at(394), 394);
    EXPECT_EQ(assembled.codestream.size(), 4101U);
    EXPECT_TRUE(assembled.complete);
    EXPECT_TRUE(assembled.endsWithEoc());
}

TEST_F(TextureAssemblerFileTest, RejectsRecordWithoutBody) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::cache::TextureAssembler assembler(*layout_);

    // Record 0 is a free slot (bodySize == 0).
    EXPECT_THROW(assembler.assemble(entries.at(0), 0), sltcd::EntryNotFound);
}

TEST_F(TextureAssemblerFileTest, RejectsMissingBodyFile) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::cache::TextureAssembler assembler(*layout_);

    // Record 100 has a body on record, but no such file in this temp cache.
    EXPECT_THROW(assembler.assemble(entries.at(100), 100), sltcd::EntryNotFound);
}

TEST_F(TextureAssemblerFileTest, RejectsCacheThatEndsBeforeTheBlock) {
    const sltcd::TextureEntries entries = miniEntries();
    const sltcd::cache::TextureAssembler assembler(*layout_);

    // The temp texture.cache holds 395 blocks, so index 395 is out of range.
    EXPECT_THROW(assembler.assemble(entries.at(394), 395), sltcd::CacheTooSmall);
}

TEST_F(TextureAssemblerFileTest, RejectsBodyFileWithUnexpectedSize) {
    sltcd::TextureEntry entry = miniEntries().at(394);
    entry.bodySize = 999; // does not match the 3501 byte file on disk

    const sltcd::cache::TextureAssembler assembler(*layout_);
    EXPECT_THROW(assembler.assemble(entry, 394), sltcd::SizeMismatch);
}

TEST_F(TextureAssemblerFileTest, RejectsABodyLargerThanTheFormatAllows) {
    sltcd::TextureEntry entry = miniEntries().at(394);
    // The file on disk is 3501 bytes and stays untouched: the record is refused
    // before it is read, so the size never has to match.
    entry.bodySize = static_cast<std::int32_t>(sltcd::CacheFormatConfig::kMaxBodySize + 1);

    const sltcd::cache::TextureAssembler assembler(*layout_);
    EXPECT_THROW(assembler.assemble(entry, 394), sltcd::InvalidFormat);
}

TEST(TextureAssembler, RejectsABodyLargerThanTheFormatAllowsInMemory) {
    const sltcd::TextureEntries entries = miniEntries();
    sltcd::TextureEntry entry = entries.at(394);
    entry.bodySize = static_cast<std::int32_t>(sltcd::CacheFormatConfig::kMaxBodySize + 1);

    const std::vector<std::uint8_t> block = sltcd::fileutils::readFile(dataFile("block_394.bin"));
    EXPECT_THROW(sltcd::cache::TextureAssembler::assembleBuffers(entry, 394, block, {}), sltcd::InvalidFormat);
}
