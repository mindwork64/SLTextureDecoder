#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

#include "cache/TextureAssembler.h"
#include "cache/TextureEntries.h"
#include "jpeg2000/Jpeg2000Decoder.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace {

std::filesystem::path dataFile(const std::string& name) {
    return std::filesystem::path(SLTCD_TEST_DATA_DIR) / name;
}

/// Rebuilds the codestream of a frozen record from the fixture halves. The
/// header block is taken from texture.cache and the body from the .texture
/// file, exactly like the production assembler does.
std::vector<std::uint8_t> assembleFixture(unsigned index) {
    const std::vector<std::uint8_t> block =
        sltcd::fileutils::readFile(dataFile("block_" + std::to_string(index) + ".bin"));
    const std::vector<std::uint8_t> body =
        sltcd::fileutils::readFile(dataFile("body_" + std::to_string(index) + ".texture"));

    sltcd::TextureEntry entry;
    entry.bodySize = static_cast<std::int32_t>(body.size());
    entry.imageSize = static_cast<std::int32_t>(block.size() + body.size());

    return sltcd::cache::TextureAssembler::assembleBuffers(entry, index, block, body).codestream;
}

void expectSamePixels(const std::vector<std::uint8_t>& expected, const std::vector<std::uint8_t>& actual) {
    ASSERT_EQ(expected.size(), actual.size());
    const auto diff = std::mismatch(expected.begin(), expected.end(), actual.begin());
    if (diff.first != expected.end()) {
        const std::size_t offset = static_cast<std::size_t>(std::distance(expected.begin(), diff.first));
        ADD_FAILURE() << "pixel data differs at byte " << offset << ": expected " << static_cast<int>(*diff.first)
                      << ", got " << static_cast<int>(*diff.second);
    }
}

} // namespace

TEST(Jpeg2000Decoder, DecodesTheReferenceRecordByteForByte) {
    const sltcd::jpeg2000::DecodedImage image = sltcd::jpeg2000::Jpeg2000Decoder::decode(assembleFixture(394));

    EXPECT_EQ(image.width, 16U);
    EXPECT_EQ(image.height, 256U);
    EXPECT_EQ(image.components, 4U);
    EXPECT_EQ(image.pixelCount(), 4096U);
    ASSERT_EQ(image.pixels.size(), 16'384U);

    // tests/data/reference_394.rgba was produced by the standalone
    // opj_decompress utility (see tools/make_reference_pixels.ps1).
    const std::vector<std::uint8_t> reference = sltcd::fileutils::readFile(dataFile("reference_394.rgba"));
    expectSamePixels(reference, image.pixels);
}

TEST(Jpeg2000Decoder, RejectsInputThatIsNotACodestream) {
    EXPECT_THROW(sltcd::jpeg2000::Jpeg2000Decoder::decode({}), sltcd::DecodeError);
    EXPECT_THROW(sltcd::jpeg2000::Jpeg2000Decoder::decode({0x00, 0x01, 0x02, 0x03}), sltcd::DecodeError);
    EXPECT_THROW(sltcd::jpeg2000::Jpeg2000Decoder::decode({'J', 'P', '2', '\0'}), sltcd::DecodeError);
}

TEST(Jpeg2000Decoder, TryDecodeReportsDiagnosticsInsteadOfThrowing) {
    std::string diagnostics;
    const auto image = sltcd::jpeg2000::Jpeg2000Decoder::tryDecode({0x00, 0x01}, {}, &diagnostics);

    EXPECT_FALSE(image.has_value());
    EXPECT_FALSE(diagnostics.empty());
}

TEST(Jpeg2000Decoder, FailsOnTruncatedRecordWithDiagnostics) {
    // Record 100 of the frozen slice only holds a prefix of its codestream
    // (see docs/format-notes.md), so a strict decode must fail cleanly.
    std::string diagnostics;
    const auto image = sltcd::jpeg2000::Jpeg2000Decoder::tryDecode(assembleFixture(100), {}, &diagnostics);

    EXPECT_FALSE(image.has_value());
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_THROW(sltcd::jpeg2000::Jpeg2000Decoder::decode(assembleFixture(100)), sltcd::DecodeError);
}

TEST(Jpeg2000Decoder, LenientModeRecoversTruncatedRecords) {
    // OpenJPEG returns the samples it could decode when it is not asked to be
    // strict, even though the cached codestream of record 100 is truncated.
    // Callers must pair this with AssembledTexture::complete / the record flags
    // instead of silently trusting the result.
    std::string diagnostics;
    const auto image = sltcd::jpeg2000::Jpeg2000Decoder::tryDecode(assembleFixture(100),
                                                                   sltcd::jpeg2000::DecodeOptions{false}, &diagnostics);

    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(image->width, 512U);
    EXPECT_EQ(image->height, 512U);
    EXPECT_EQ(image->components, 3U);
    EXPECT_EQ(image->pixels.size(), 512U * 512U * 3U);

    // The truncation must be visible in the diagnostics.
    EXPECT_FALSE(diagnostics.empty());
    EXPECT_NE(diagnostics.find("inconsistent"), std::string::npos);
}
