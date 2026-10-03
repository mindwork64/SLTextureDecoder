#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>
#include <unordered_set>

#include "utils/UUID.h"

namespace {

// A UUID taken from a real texture.entries record.
constexpr std::string_view kRealUuid = "4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4c";

} // namespace

TEST(UUID, DefaultConstructedIsAllZero) {
    const sltcd::UUID uuid;
    EXPECT_EQ(uuid.toString(), "00000000-0000-0000-0000-000000000000");
}

TEST(UUID, ParsesCanonicalFormAndRoundTrips) {
    const auto parsed = sltcd::UUID::fromString(kRealUuid);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->toString(), kRealUuid);
}

TEST(UUID, KeepsBytesInCanonicalBigEndianOrder) {
    const auto parsed = sltcd::UUID::fromString(kRealUuid);
    ASSERT_TRUE(parsed.has_value());

    const sltcd::UUID::Bytes expected{
        0x4a, 0x2e, 0x79, 0xd1, 0x1b, 0x0e, 0x0f, 0x94,
        0xf0, 0x6e, 0xd1, 0x24, 0x4d, 0x3d, 0x0a, 0x4c};
    EXPECT_EQ(parsed->bytes(), expected);
}

TEST(UUID, FromBytesMatchesFromString) {
    const sltcd::UUID::Bytes raw{
        0x4a, 0x2e, 0x79, 0xd1, 0x1b, 0x0e, 0x0f, 0x94,
        0xf0, 0x6e, 0xd1, 0x24, 0x4d, 0x3d, 0x0a, 0x4c};

    const auto fromBytes = sltcd::UUID::fromBytes(raw);
    const auto fromString = sltcd::UUID::fromString(kRealUuid);
    ASSERT_TRUE(fromString.has_value());
    EXPECT_EQ(fromBytes, *fromString);
}

TEST(UUID, AcceptsUppercaseAndBracesAndSurroundingSpace) {
    const auto upper = sltcd::UUID::fromString("4A2E79D1-1B0E-0F94-F06E-D1244D3D0A4C");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(upper->toString(), kRealUuid);

    const auto braced = sltcd::UUID::fromString("  {4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4c}  ");
    ASSERT_TRUE(braced.has_value());
    EXPECT_EQ(*braced, *upper);
}

TEST(UUID, RejectsMalformedInput) {
    EXPECT_FALSE(sltcd::UUID::fromString("").has_value());
    EXPECT_FALSE(sltcd::UUID::fromString("not-a-uuid").has_value());
    // Too short.
    EXPECT_FALSE(sltcd::UUID::fromString("4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4").has_value());
    // Too long.
    EXPECT_FALSE(sltcd::UUID::fromString("4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4cc").has_value());
    // Non-hex characters.
    EXPECT_FALSE(sltcd::UUID::fromString("4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4z").has_value());
    // Dashes in the wrong positions.
    EXPECT_FALSE(sltcd::UUID::fromString("4a2e79d11-1b0e-0f94-f06e-d1244d3d0a4c").has_value());
    // Unbalanced braces.
    EXPECT_FALSE(sltcd::UUID::fromString("{4a2e79d1-1b0e-0f94-f06e-d1244d3d0a4c").has_value());
}

TEST(UUID, ParseThrowsOnInvalidInput) {
    EXPECT_THROW(sltcd::UUID::parse("bogus"), std::invalid_argument);
    EXPECT_NO_THROW(sltcd::UUID::parse(kRealUuid));
}

TEST(UUID, EqualityAndHashSupportContainers) {
    const auto a = sltcd::UUID::parse(kRealUuid);
    const auto b = sltcd::UUID::parse(kRealUuid);
    const auto c = sltcd::UUID::parse("00000000-0000-0000-0000-000000000001");

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(std::hash<sltcd::UUID>{}(a), std::hash<sltcd::UUID>{}(b));

    std::unordered_set<sltcd::UUID> set{a, b, c};
    EXPECT_EQ(set.size(), 2U);
}

TEST(UUID, OrdersLexicographicallyByBytes) {
    const auto low = sltcd::UUID::parse("00000000-0000-0000-0000-000000000000");
    const auto high = sltcd::UUID::parse("ffffffff-ffff-ffff-ffff-ffffffffffff");
    EXPECT_TRUE(low < high);
    EXPECT_FALSE(high < low);
}
