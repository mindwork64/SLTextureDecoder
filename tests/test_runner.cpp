#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <png.h>

#include "TestCacheFixture.h"
#include "cache/CacheLayout.h"
#include "cli/CliOptions.h"
#include "cli/Runner.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"
#include "utils/Interrupt.h"
#include "utils/Logger.h"

namespace {

using sltcd::cli::CliOptions;
using sltcd::cli::RunSummary;

/// A request must never leak into another test, whatever the test does.
class CleanInterrupts {
public:
    CleanInterrupts() { sltcd::interrupt::reset(); }
    ~CleanInterrupts() { sltcd::interrupt::reset(); }
};

std::filesystem::path outDirOf(const std::filesystem::path& cacheDir) {
    return cacheDir / "png";
}

std::filesystem::path expectedFile(const std::filesystem::path& cacheDir, const std::string& uuid, bool complete) {
    return sltcd::cli::outputFile(outDirOf(cacheDir), sltcd::UUID::parse(uuid), complete);
}

/// Colour type field of the IHDR chunk: signature(8) + length(4) + "IHDR"(4) +
/// width(4) + height(4) + bit depth(1) -> colour type.
std::uint8_t pngColorType(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = sltcd::fileutils::readFile(path);
    return bytes.at(8 + 4 + 4 + 4 + 4 + 1);
}

} // namespace

TEST(Runner, WritesThePngOfASingleTexture) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_single");
    sltcd::Logger logger;

    const RunSummary summary = sltcd::cli::run(
        CliOptions::parse({"--cache-dir", fixture.root().string(), "--id", sltcd::test::kUuidRecord394}), logger);

    EXPECT_EQ(summary.records, 395U);
    EXPECT_EQ(summary.selected, 1U);
    EXPECT_EQ(summary.written, 1U);
    EXPECT_EQ(summary.partial, 0U);
    EXPECT_EQ(summary.failed, 0U);
    EXPECT_FALSE(summary.interrupted);

    const std::filesystem::path png = expectedFile(fixture.root(), sltcd::test::kUuidRecord394, true);
    EXPECT_TRUE(std::filesystem::exists(png));
    EXPECT_GT(sltcd::fileutils::size(png), 0U);

    // The file appears through a rename, so no temporary file may survive.
    EXPECT_FALSE(std::filesystem::exists(sltcd::fileutils::temporaryPath(png)));
}

TEST(Runner, MarksBestEffortResultsAsPartialAndRespectsOverwrite) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_partial");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();

    const RunSummary first = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--index", "100"}), logger);
    EXPECT_EQ(first.written, 1U);
    EXPECT_EQ(first.partial, 1U);
    EXPECT_TRUE(std::filesystem::exists(expectedFile(fixture.root(), sltcd::test::kUuidRecord100, false)));

    // Second run: the PNG is there, so the record is skipped unless asked to
    // overwrite.
    const RunSummary second = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--index", "100"}), logger);
    EXPECT_EQ(second.written, 0U);
    EXPECT_EQ(second.skipped, 1U);
    EXPECT_EQ(second.failed, 0U);

    const RunSummary third =
        sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--index", "100", "--overwrite"}), logger);
    EXPECT_EQ(third.written, 1U);
    EXPECT_EQ(third.skipped, 0U);
}

TEST(Runner, CountsARecordWithoutABodyAsFailed) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_free_slot");
    sltcd::Logger logger;

    const RunSummary summary =
        sltcd::cli::run(CliOptions::parse({"--cache-dir", fixture.root().string(), "--index", "0"}), logger);

    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.failed, 1U);
}

TEST(Runner, ReportsAnIndexOutsideTheRecordArray) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_bad_index");
    sltcd::Logger logger;

    EXPECT_THROW(sltcd::cli::run(CliOptions::parse({"--cache-dir", fixture.root().string(), "--index", "395"}), logger),
                 sltcd::EntryNotFound);
}

TEST(Runner, ReportsAMissingCacheDirectory) {
    sltcd::Logger logger;
    EXPECT_THROW(sltcd::cli::run(CliOptions::parse({"--cache-dir", "D:/sltcd/no-cache"}), logger),
                 sltcd::CacheNotFound);
}

TEST(Runner, KeepJ2kStoresTheAssembledCodestream) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_keep_j2k");
    sltcd::Logger logger;

    const RunSummary summary = sltcd::cli::run(
        CliOptions::parse({"--cache-dir", fixture.root().string(), "--id", sltcd::test::kUuidRecord394, "--keep-j2k"}),
        logger);
    EXPECT_EQ(summary.written, 1U);

    const std::filesystem::path j2c =
        sltcd::cli::codestreamFile(outDirOf(fixture.root()), sltcd::UUID::parse(sltcd::test::kUuidRecord394), true);
    ASSERT_TRUE(std::filesystem::exists(j2c));

    // The stored codestream is exactly the header block followed by the body.
    std::vector<std::uint8_t> expected = sltcd::fileutils::readFile(sltcd::test::dataFile("block_394.bin"));
    const std::vector<std::uint8_t> body = sltcd::fileutils::readFile(sltcd::test::dataFile("body_394.texture"));
    expected.insert(expected.end(), body.begin(), body.end());
    EXPECT_EQ(sltcd::fileutils::readFile(j2c), expected);

    EXPECT_TRUE(std::filesystem::exists(expectedFile(fixture.root(), sltcd::test::kUuidRecord394, true)));
}

TEST(Runner, NoAlphaWritesAnRgbPngWithoutAlphaChannel) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_no_alpha");
    sltcd::Logger logger;

    const RunSummary summary = sltcd::cli::run(
        CliOptions::parse({"--cache-dir", fixture.root().string(), "--id", sltcd::test::kUuidRecord394, "--no-alpha"}),
        logger);
    EXPECT_EQ(summary.written, 1U);

    const std::filesystem::path png = expectedFile(fixture.root(), sltcd::test::kUuidRecord394, true);
    ASSERT_TRUE(std::filesystem::exists(png));
    EXPECT_EQ(pngColorType(png), static_cast<std::uint8_t>(PNG_COLOR_TYPE_RGB));
}

TEST(Runner, WritesNoCodestreamWithoutKeepJ2k) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_without_keep_j2k");
    sltcd::Logger logger;

    sltcd::cli::run(CliOptions::parse({"--cache-dir", fixture.root().string(), "--id", sltcd::test::kUuidRecord394}),
                    logger);

    const std::filesystem::path j2c =
        sltcd::cli::codestreamFile(outDirOf(fixture.root()), sltcd::UUID::parse(sltcd::test::kUuidRecord394), true);
    EXPECT_FALSE(std::filesystem::exists(j2c));
}

TEST(Runner, NeverTouchesTheCacheItself) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_read_only");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();
    const sltcd::cache::CacheLayout layout(fixture.root());

    // The tool promises a read-only cache (README, NOTICE.md): after a full run
    // both cache files must be byte for byte what they were.
    const std::vector<std::uint8_t> entriesBefore = sltcd::fileutils::readFile(layout.entriesFile());
    const std::vector<std::uint8_t> headerBefore = sltcd::fileutils::readFile(layout.headerCacheFile());

    sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--keep-j2k"}), logger);
    sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--overwrite"}), logger);

    EXPECT_EQ(sltcd::fileutils::readFile(layout.entriesFile()), entriesBefore);
    EXPECT_EQ(sltcd::fileutils::readFile(layout.headerCacheFile()), headerBefore);
}

TEST(Runner, DryRunCountsWithoutWritingAnything) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_dry_run");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();

    const RunSummary summary = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--dry-run"}), logger);

    // The fixture announces a body for records 100 and 394 only.
    EXPECT_EQ(summary.records, 395U);
    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 2U);
    EXPECT_EQ(summary.partial, 1U); // record 100 only holds a truncated codestream
    EXPECT_EQ(summary.skipped, 0U);
    EXPECT_EQ(summary.failed, 0U);

    // Not a single file and not even the output directory.
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture.root())));
}

TEST(Runner, DryRunIsTheSameRunAsTheRealOne) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_dry_run_same");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();

    const RunSummary planned = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--dry-run"}), logger);
    const RunSummary done = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache}), logger);

    EXPECT_EQ(planned.written, done.written);
    EXPECT_EQ(planned.partial, done.partial);
    EXPECT_EQ(planned.skipped, done.skipped);
    EXPECT_EQ(planned.failed, done.failed);
    EXPECT_TRUE(std::filesystem::exists(expectedFile(fixture.root(), sltcd::test::kUuidRecord394, true)));
}

TEST(Runner, DryRunAlsoReportsSkippedFiles) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_dry_run_skipped");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();

    sltcd::cli::run(CliOptions::parse({"--cache-dir", cache}), logger); // writes both PNGs
    const RunSummary summary = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--dry-run"}), logger);

    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.skipped, 2U);
}

TEST(Runner, DryRunReportsARecordWithoutABodyAsFailed) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_dry_run_free_slot");
    sltcd::Logger logger;

    // Record 0 is a free slot (bodySize == 0) with a stale body file on disk.
    const RunSummary summary = sltcd::cli::run(
        CliOptions::parse({"--cache-dir", fixture.root().string(), "--index", "0", "--dry-run"}), logger);

    EXPECT_EQ(summary.selected, 1U);
    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.failed, 1U);
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture.root())));
}

TEST(Runner, DryRunReportsAMissingBodyFileAsFailed) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_dry_run_missing_body");
    sltcd::Logger logger;
    const std::string cache = fixture.root().string();

    // The record announces a body of 3501 bytes, the file is gone: a dry run
    // has to predict the failure the real run would hit.
    ASSERT_TRUE(std::filesystem::remove(
        sltcd::cache::CacheLayout(fixture.root()).bodyFile(sltcd::UUID::parse(sltcd::test::kUuidRecord394))));

    const RunSummary summary = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache, "--dry-run"}), logger);

    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 1U);
    EXPECT_EQ(summary.failed, 1U);

    const RunSummary done = sltcd::cli::run(CliOptions::parse({"--cache-dir", cache}), logger);
    EXPECT_EQ(done.written, summary.written);
    EXPECT_EQ(done.failed, summary.failed);
}

TEST(Runner, StopsBetweenRecordsWhenInterrupted) {
    const sltcd::test::CacheFixture fixture("sltcd_runner_interrupted");
    sltcd::Logger logger;
    const CleanInterrupts clean;

    sltcd::interrupt::request();

    const RunSummary summary = sltcd::cli::run(CliOptions::parse({"--cache-dir", fixture.root().string()}), logger);

    EXPECT_TRUE(summary.interrupted);
    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.failed, 0U);
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture.root())));
}
