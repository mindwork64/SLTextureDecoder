#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include "TestCacheFixture.h"
#include "cli/CliOptions.h"
#include "cli/Runner.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

namespace {

using sltcd::cli::CliOptions;
using sltcd::cli::RunSummary;

std::filesystem::path outDirOf(const std::filesystem::path& cacheDir) {
    return cacheDir / "png";
}

std::filesystem::path expectedFile(const std::filesystem::path& cacheDir, const std::string& uuid, bool complete) {
    return sltcd::cli::outputFile(outDirOf(cacheDir), sltcd::UUID::parse(uuid), complete);
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

    const std::filesystem::path png = expectedFile(fixture.root(), sltcd::test::kUuidRecord394, true);
    EXPECT_TRUE(std::filesystem::exists(png));
    EXPECT_GT(sltcd::fileutils::size(png), 0U);
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
