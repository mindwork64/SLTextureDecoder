#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "cache/TextureEntries.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace {

std::filesystem::path dataFile(const std::string& name) {
    return std::filesystem::path(SLTCD_TEST_DATA_DIR) / name;
}

/// Values below come from tests/data/manifest.txt, which is generated from a
/// real cache by tools/make_test_slice.ps1.
constexpr std::uint32_t kRealEntriesCount = 196'700;

constexpr const char* kUuidRecord0 = "e569711a-27c2-aad4-9246-0c910239a179";
constexpr const char* kUuidRecord100 = "87d1503c-9713-3228-9672-0a019a956b5e";
constexpr const char* kUuidRecord394 = "6ffba690-3dd3-3e31-0913-df429d3204c6";

std::vector<std::uint8_t> realHeader() {
    return sltcd::fileutils::readFile(dataFile("entries_header_real.bin"));
}

} // namespace

TEST(TextureEntries, ParsesRealHeader) {
    const sltcd::EntriesInfo info = sltcd::TextureEntries::parseInfo(realHeader());

    EXPECT_FLOAT_EQ(info.version, 1.71F);
    EXPECT_EQ(info.addressSize, 64U);
    EXPECT_EQ(info.encoderVersion, "KDU v8.4.1");
    EXPECT_EQ(info.entriesCount, kRealEntriesCount);
    EXPECT_EQ(sltcd::TextureEntries::expectedFileSize(info.entriesCount), 5'507'644ULL);
}

TEST(TextureEntries, ParsesMiniFileWithRealRecords) {
    const sltcd::TextureEntries entries = sltcd::TextureEntries::load(dataFile("entries_mini.bin"));

    ASSERT_EQ(entries.size(), 395U);
    EXPECT_EQ(entries.info().entriesCount, 395U);
    EXPECT_EQ(entries.info().encoderVersion, "KDU v8.4.1");

    const sltcd::TextureEntry& free = entries.at(0);
    EXPECT_EQ(free.id.toString(), kUuidRecord0);
    EXPECT_EQ(free.imageSize, -1);
    EXPECT_EQ(free.bodySize, 0);
    EXPECT_EQ(free.timestamp, 1'784'558'438U);
    EXPECT_FALSE(free.hasBody());
    EXPECT_TRUE(free.hasUnknownImageSize());

    const sltcd::TextureEntry& partial = entries.at(100);
    EXPECT_EQ(partial.id.toString(), kUuidRecord100);
    EXPECT_EQ(partial.imageSize, 1537);
    EXPECT_EQ(partial.bodySize, 936);
    EXPECT_EQ(partial.timestamp, 1'789'645'469U);
    EXPECT_TRUE(partial.hasBody());
    EXPECT_FALSE(partial.isComplete());
    EXPECT_TRUE(partial.isPartial());
    EXPECT_EQ(partial.assembledSize(), 1536);

    const sltcd::TextureEntry& complete = entries.at(394);
    EXPECT_EQ(complete.id.toString(), kUuidRecord394);
    EXPECT_EQ(complete.imageSize, 4101);
    EXPECT_EQ(complete.bodySize, 3501);
    EXPECT_EQ(complete.timestamp, 1'789'295'787U);
    EXPECT_TRUE(complete.hasBody());
    EXPECT_TRUE(complete.isComplete());
    EXPECT_FALSE(complete.isPartial());
    EXPECT_EQ(complete.assembledSize(), 4101);
}

TEST(TextureEntries, CompleteRecordMatchesAssembledSize) {
    const sltcd::TextureEntries entries = sltcd::TextureEntries::load(dataFile("entries_mini.bin"));
    const sltcd::TextureEntry& complete = entries.at(394);

    // For a complete record the announced image size equals header + body.
    EXPECT_EQ(complete.imageSize, complete.assembledSize());
}

TEST(TextureEntries, RejectsTruncatedFile) {
    EXPECT_THROW(sltcd::TextureEntries::load(dataFile("entries_truncated.bin")), sltcd::InvalidFormat);

    try {
        sltcd::TextureEntries::load(dataFile("entries_truncated.bin"));
        FAIL() << "expected InvalidFormat";
    } catch (const sltcd::Error& error) {
        EXPECT_EQ(error.code(), sltcd::ErrorCode::InvalidFormat);
        EXPECT_NE(std::string(error.what()).find("truncated"), std::string::npos);
    }
}

TEST(TextureEntries, RejectsInputSmallerThanHeader) {
    EXPECT_THROW(sltcd::TextureEntries::parse({}), sltcd::InvalidFormat);
    EXPECT_THROW(sltcd::TextureEntries::parse(std::vector<std::uint8_t>(43, 0)), sltcd::InvalidFormat);
    EXPECT_NO_THROW(sltcd::TextureEntries::parseInfo(realHeader()));
}

TEST(TextureEntries, RejectsImplausibleEntryCount) {
    std::vector<std::uint8_t> header = realHeader();
    // Patch mEntries with a value far above CacheFormatConfig::kMaxReasonableEntries.
    header[40] = 0xFF;
    header[41] = 0xFF;
    header[42] = 0xFF;
    header[43] = 0x7F;

    EXPECT_THROW(sltcd::TextureEntries::parseInfo(header), sltcd::InvalidFormat);
    EXPECT_THROW(sltcd::TextureEntries::parse(header), sltcd::InvalidFormat);
}

TEST(TextureEntries, AcceptsFileWithTrailingSlack) {
    // The viewer preallocates the record array, so a file may legitimately be
    // longer than the header announces; only the announced records are read.
    std::vector<std::uint8_t> bytes = sltcd::fileutils::readFile(dataFile("entries_mini.bin"));
    bytes.resize(bytes.size() + 28, 0);

    const sltcd::TextureEntries entries = sltcd::TextureEntries::parse(bytes);
    EXPECT_EQ(entries.size(), 395U);
}

TEST(TextureEntries, IndexOfFindsRecordById) {
    const sltcd::TextureEntries entries = sltcd::TextureEntries::load(dataFile("entries_mini.bin"));

    const auto found = entries.indexOf(sltcd::UUID::parse(kUuidRecord394));
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, 394U);

    EXPECT_FALSE(entries.indexOf(sltcd::UUID::parse("00000000-0000-0000-0000-000000000001")).has_value());
}

TEST(TextureEntries, AtThrowsOnInvalidIndex) {
    const sltcd::TextureEntries entries = sltcd::TextureEntries::load(dataFile("entries_mini.bin"));
    EXPECT_THROW(entries.at(395), std::out_of_range);
    EXPECT_NO_THROW(entries.at(394));
}

TEST(TextureEntries, FrozenSliceStaysInSyncWithRecords) {
    const sltcd::TextureEntries entries = sltcd::TextureEntries::load(dataFile("entries_mini.bin"));

    // Header blocks are always exactly one record in size.
    EXPECT_EQ(std::filesystem::file_size(dataFile("block_0.bin")), 600U);
    EXPECT_EQ(std::filesystem::file_size(dataFile("block_100.bin")), 600U);
    EXPECT_EQ(std::filesystem::file_size(dataFile("block_394.bin")), 600U);

    // Body files match the bodySize of their record.
    EXPECT_EQ(std::filesystem::file_size(dataFile("body_394.texture")),
              static_cast<std::uintmax_t>(entries.at(394).bodySize));
    EXPECT_EQ(std::filesystem::file_size(dataFile("body_100.texture")),
              static_cast<std::uintmax_t>(entries.at(100).bodySize));

    // Record 0 is a free slot, yet its orphan body file still exists on disk -
    // a real cache keeps such leftovers until the slot is reused.
    EXPECT_FALSE(entries.at(0).hasBody());
    EXPECT_EQ(std::filesystem::file_size(dataFile("body_0.texture")), 31'875U);
}

