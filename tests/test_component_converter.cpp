#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "jpeg2000/ComponentConverter.h"
#include "utils/Errors.h"

namespace {

sltcd::jpeg2000::DecodedImage imageOf(std::uint32_t width, std::uint32_t height, std::uint32_t components,
                                      std::vector<std::uint8_t> samples) {
    sltcd::jpeg2000::DecodedImage image;
    image.width = width;
    image.height = height;
    image.components = components;
    image.pixels = std::move(samples);
    return image;
}

} // namespace

TEST(ComponentConverter, MapsThreeComponentsToOpaqueRgb) {
    const sltcd::jpeg2000::DecodedImage image = imageOf(2, 1, 3, {10, 20, 30, 40, 50, 60});
    EXPECT_EQ(sltcd::jpeg2000::toRgba(image), (std::vector<std::uint8_t>{10, 20, 30, 255, 40, 50, 60, 255}));
}

TEST(ComponentConverter, KeepsTheAlphaOfFourComponents) {
    const sltcd::jpeg2000::DecodedImage image = imageOf(1, 1, 4, {1, 2, 3, 4});
    EXPECT_EQ(sltcd::jpeg2000::toRgba(image), (std::vector<std::uint8_t>{1, 2, 3, 4}));
}

TEST(ComponentConverter, ExpandsOneAndTwoComponentsToGrayscale) {
    const sltcd::jpeg2000::DecodedImage luminance = imageOf(1, 1, 1, {7});
    EXPECT_EQ(sltcd::jpeg2000::toRgba(luminance), (std::vector<std::uint8_t>{7, 7, 7, 255}));

    const sltcd::jpeg2000::DecodedImage luminanceAlpha = imageOf(1, 1, 2, {9, 128});
    EXPECT_EQ(sltcd::jpeg2000::toRgba(luminanceAlpha), (std::vector<std::uint8_t>{9, 9, 9, 128}));
}

TEST(ComponentConverter, DropsSurplusComponentsButKeepsTheAlpha) {
    const sltcd::jpeg2000::DecodedImage image = imageOf(1, 1, 5, {1, 2, 3, 4, 5});

    std::string warning;
    EXPECT_EQ(sltcd::jpeg2000::toRgba(image, &warning), (std::vector<std::uint8_t>{1, 2, 3, 4}));
    EXPECT_NE(warning.find("5 components"), std::string::npos);
}

TEST(ComponentConverter, RejectsInconsistentSampleCount) {
    const sltcd::jpeg2000::DecodedImage image = imageOf(2, 2, 3, {1, 2, 3});
    EXPECT_THROW(sltcd::jpeg2000::toRgba(image), sltcd::DecodeError);
}
