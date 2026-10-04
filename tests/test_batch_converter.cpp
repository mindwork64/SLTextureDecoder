#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "TestCacheFixture.h"
#include "batch/BatchConverter.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"
#include "utils/Interrupt.h"
#include "utils/Logger.h"

namespace {

using sltcd::batch::BatchOptions;
using sltcd::batch::BatchSummary;

/// A request must never leak into another test, whatever the test does.
class CleanInterrupts {
public:
    CleanInterrupts() { sltcd::interrupt::reset(); }
    ~CleanInterrupts() { sltcd::interrupt::reset(); }
};

BatchOptions optionsOf(const sltcd::test::CacheFixture& fixture) {
    BatchOptions options;
    options.cacheDir = fixture.root();
    options.jobs = 2;
    return options;
}

std::filesystem::path outDirOf(const sltcd::test::CacheFixture& fixture) {
    return fixture.root() / "png";
}

} // namespace

TEST(BatchConverter, WritesTheSelectionWithJobs) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_run");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.limit = 2;

    const BatchSummary summary = sltcd::batch::run(options, logger);

    // The fixture announces a body for records 100 and 394 only, so both are
    // selected and both have a body file.
    EXPECT_EQ(summary.records, 395U);
    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 2U);
    EXPECT_EQ(summary.failed, 0U);
    EXPECT_FALSE(summary.cancelled);
    EXPECT_FALSE(summary.interrupted);

    const std::filesystem::path partial =
        sltcd::batch::pngFile(summary.outDir, sltcd::UUID::parse(sltcd::test::kUuidRecord100), false);
    const std::filesystem::path complete =
        sltcd::batch::pngFile(summary.outDir, sltcd::UUID::parse(sltcd::test::kUuidRecord394), true);
    EXPECT_TRUE(std::filesystem::exists(complete));
    EXPECT_TRUE(std::filesystem::exists(partial));
    EXPECT_EQ(summary.bytes, sltcd::fileutils::size(complete) + sltcd::fileutils::size(partial));
    EXPECT_NE(summary.bytes, 0U);
}

TEST(BatchConverter, DryRunCountsWithoutWritingAnything) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_dry_run");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.dryRun = true;

    const BatchSummary summary = sltcd::batch::run(options, logger);

    EXPECT_EQ(summary.selected, 2U);
    EXPECT_EQ(summary.written, 2U);
    EXPECT_EQ(summary.partial, 1U); // record 100 only holds a truncated codestream
    EXPECT_EQ(summary.failed, 0U);
    EXPECT_EQ(summary.bytes, 0U);
    // Nothing was created: not a single PNG and not even the output directory.
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture)));
}

TEST(BatchConverter, DryRunIsTheSameRunAsTheRealOne) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_dry_run_same");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.dryRun = true;
    const BatchSummary planned = sltcd::batch::run(options, logger);

    options.dryRun = false;
    const BatchSummary done = sltcd::batch::run(options, logger);

    EXPECT_EQ(planned.written, done.written);
    EXPECT_EQ(planned.partial, done.partial);
    EXPECT_EQ(planned.skipped, done.skipped);
    EXPECT_EQ(planned.failed, done.failed);
    EXPECT_GT(done.bytes, 0U);
}

TEST(BatchConverter, DryRunReportsARecordWithoutABodyAsFailed) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_dry_run_free_slot");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.dryRun = true;
    options.onlyIndex = 0; // a free slot with a stale body file on disk

    const BatchSummary summary = sltcd::batch::run(options, logger);

    EXPECT_EQ(summary.selected, 1U);
    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.failed, 1U);
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture)));
}

TEST(BatchConverter, DryRunReportsAMissingBodyFileAsFailed) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_dry_run_missing_body");
    sltcd::Logger logger;

    ASSERT_TRUE(std::filesystem::remove(
        sltcd::cache::CacheLayout(fixture.root()).bodyFile(sltcd::UUID::parse(sltcd::test::kUuidRecord394))));

    BatchOptions options = optionsOf(fixture);
    options.dryRun = true;
    const BatchSummary planned = sltcd::batch::run(options, logger);

    options.dryRun = false;
    const BatchSummary done = sltcd::batch::run(options, logger);

    EXPECT_EQ(planned.written, done.written);
    EXPECT_EQ(planned.failed, done.failed);
    EXPECT_EQ(planned.failed, 1U);
}

TEST(BatchConverter, SkipsExistingPngsUnlessAskedToOverwrite) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_overwrite");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.limit = 1;
    options.dryRun = true;
    sltcd::batch::run(options, logger); // only counts here

    options.dryRun = false;
    const BatchSummary first = sltcd::batch::run(options, logger);
    EXPECT_EQ(first.skipped, 0U);

    const BatchSummary second = sltcd::batch::run(options, logger);
    EXPECT_EQ(second.written, 0U);
    EXPECT_EQ(second.skipped, 1U);

    options.overwrite = true;
    const BatchSummary third = sltcd::batch::run(options, logger);
    EXPECT_EQ(third.written, 1U);
    EXPECT_EQ(third.skipped, 0U);
}

TEST(BatchConverter, StopsBetweenRecordsWhenInterrupted) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_interrupted");
    sltcd::Logger logger;
    const CleanInterrupts clean;

    sltcd::interrupt::request();

    const BatchSummary summary = sltcd::batch::run(optionsOf(fixture), logger);

    EXPECT_TRUE(summary.interrupted);
    EXPECT_TRUE(summary.cancelled); // the GUI treats both as "stopped"
    EXPECT_EQ(summary.written, 0U);
    EXPECT_EQ(summary.failed, 0U);
    EXPECT_FALSE(std::filesystem::exists(outDirOf(fixture)));
}

TEST(BatchConverter, StopsWhenTheCallbackAsksForIt) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_callback_cancel");
    sltcd::Logger logger;

    sltcd::batch::BatchCallbacks callbacks;
    callbacks.isCancelled = []() { return true; };

    const BatchSummary summary = sltcd::batch::run(optionsOf(fixture), logger, callbacks);

    EXPECT_TRUE(summary.cancelled);
    EXPECT_FALSE(summary.interrupted); // the user did not interrupt this one
    EXPECT_EQ(summary.written, 0U);
}

TEST(BatchConverter, RefusesAMissingCacheDirectory) {
    sltcd::Logger logger;

    BatchOptions options;
    options.cacheDir = "D:/sltcd/no-cache";

    EXPECT_THROW(sltcd::batch::run(options, logger), sltcd::CacheNotFound);
}

TEST(BatchConverter, ReportsAnIndexOutsideTheRecordArray) {
    const sltcd::test::CacheFixture fixture("sltcd_batch_bad_index");
    sltcd::Logger logger;

    BatchOptions options = optionsOf(fixture);
    options.onlyIndex = 395;

    EXPECT_THROW(sltcd::batch::run(options, logger), sltcd::EntryNotFound);
}
