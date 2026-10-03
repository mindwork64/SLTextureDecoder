#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "cli/CliOptions.h"
#include "utils/Errors.h"

namespace {

using sltcd::cli::CliOptions;

} // namespace

TEST(CliOptions, ParsesACacheDirectoryAndDefaults) {
    const CliOptions options = CliOptions::parse({"--cache-dir", "D:/cache"});

    EXPECT_EQ(options.action, CliOptions::Action::Decode);
    EXPECT_EQ(options.cacheDir, std::filesystem::path("D:/cache"));
    EXPECT_EQ(options.outputDirectory(), std::filesystem::path("D:/cache") / "png");
    EXPECT_FALSE(options.completeOnly);
    EXPECT_FALSE(options.overwrite);
    EXPECT_FALSE(options.verbose);
    EXPECT_FALSE(options.id.has_value());
    EXPECT_FALSE(options.limit.has_value());
}

TEST(CliOptions, ParsesFiltersAndFlags) {
    const CliOptions options = CliOptions::parse({"--cache-dir", "D:/cache", "--out-dir", "D:/out", "--id",
                                                  "6ffba690-3dd3-3e31-0913-df429d3204c6", "--limit", "12",
                                                  "--complete-only", "--overwrite", "-v"});

    EXPECT_EQ(options.action, CliOptions::Action::Decode);
    EXPECT_EQ(options.outputDirectory(), std::filesystem::path("D:/out"));
    ASSERT_TRUE(options.id.has_value());
    EXPECT_EQ(options.id->toString(), "6ffba690-3dd3-3e31-0913-df429d3204c6");
    ASSERT_TRUE(options.limit.has_value());
    EXPECT_EQ(*options.limit, 12U);
    EXPECT_TRUE(options.completeOnly);
    EXPECT_TRUE(options.overwrite);
    EXPECT_TRUE(options.verbose);
}

TEST(CliOptions, ParsesASingleRecordIndex) {
    const CliOptions options = CliOptions::parse({"--cache-dir", "D:/cache", "--index", "394"});
    ASSERT_TRUE(options.index.has_value());
    EXPECT_EQ(*options.index, 394U);
}

TEST(CliOptions, HelpAndVersionShortCircuit) {
    EXPECT_EQ(CliOptions::parse({"--help"}).action, CliOptions::Action::Help);
    EXPECT_EQ(CliOptions::parse({"-V"}).action, CliOptions::Action::Version);
    EXPECT_EQ(CliOptions::parse({"--cache-dir", "D:/cache", "--help"}).action, CliOptions::Action::Help);
}

TEST(CliOptions, RejectsUnusableCommandLines) {
    EXPECT_THROW(CliOptions::parse({"nonsense"}), sltcd::UsageError);
    EXPECT_THROW(CliOptions::parse({"--cache-dir"}), sltcd::UsageError);
    EXPECT_THROW(CliOptions::parse({"--out-dir", "D:/out"}), sltcd::UsageError);
    EXPECT_THROW(CliOptions::parse({"--cache-dir", "D:/cache", "--limit", "many"}), sltcd::UsageError);
    EXPECT_THROW(CliOptions::parse({"--cache-dir", "D:/cache", "--id", "not-a-uuid"}), sltcd::UsageError);
}
