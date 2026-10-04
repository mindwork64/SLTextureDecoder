#include <gtest/gtest.h>

#include <png.h>

#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "png/PngWriter.h"
#include "utils/Errors.h"

namespace {

struct LoadedPng {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    int bitDepth = 0;
    int colorType = 0;
    std::vector<std::uint8_t> rgba;
};

/// Minimal reader; it exists to prove that PngWriter emits a real PNG file.
LoadedPng readPng(const std::filesystem::path& path) {
    std::FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr) {
        throw std::runtime_error("cannot open " + path.string());
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png == nullptr ? nullptr : png_create_info_struct(png);
    if (png == nullptr || info == nullptr) {
        std::fclose(file);
        throw std::runtime_error("libpng is out of memory");
    }
    if (setjmp(png_jmpbuf(png)) != 0) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        throw std::runtime_error("libpng cannot read " + path.string());
    }

    png_init_io(png, file);
    png_read_info(png, info);

    LoadedPng result;
    result.width = png_get_image_width(png, info);
    result.height = png_get_image_height(png, info);
    result.bitDepth = png_get_bit_depth(png, info);
    result.colorType = png_get_color_type(png, info);

    const png_byte channels = png_get_channels(png, info);
    result.rgba.resize(static_cast<std::size_t>(result.width) * result.height * channels);

    std::vector<png_bytep> rows(result.height);
    for (std::uint32_t y = 0; y < result.height; ++y) {
        rows[y] = result.rgba.data() + static_cast<std::size_t>(y) * result.width * channels;
    }
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);
    return result;
}

std::filesystem::path scratchFile(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "sltcd_png_writer_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir / name;
}

} // namespace

TEST(PngWriter, WritesAnRgbaImageThatReadsBackUnchanged) {
    // 3x2 image with a distinct value per byte and one translucent pixel.
    const std::uint32_t width = 3;
    const std::uint32_t height = 2;
    std::vector<std::uint8_t> rgba(width * height * 4);
    for (std::size_t i = 0; i < rgba.size(); ++i) {
        rgba[i] = static_cast<std::uint8_t>(i * 7);
    }
    rgba[7] = 128;

    const std::filesystem::path path = scratchFile("sample.png");
    sltcd::png::writeRgba(path, rgba, width, height);

    ASSERT_TRUE(std::filesystem::exists(path));
    const LoadedPng loaded = readPng(path);
    EXPECT_EQ(loaded.width, width);
    EXPECT_EQ(loaded.height, height);
    EXPECT_EQ(loaded.bitDepth, 8);
    EXPECT_EQ(loaded.colorType, PNG_COLOR_TYPE_RGB_ALPHA);
    EXPECT_EQ(loaded.rgba, rgba);

    std::filesystem::remove_all(path.parent_path());
}

TEST(PngWriter, RejectsABufferThatDoesNotMatchTheSize) {
    const std::filesystem::path path = scratchFile("bad.png");
    EXPECT_THROW(sltcd::png::writeRgba(path, std::vector<std::uint8_t>(10, 0), 3, 2), sltcd::WriteError);
    EXPECT_FALSE(std::filesystem::exists(path));

    std::filesystem::remove_all(path.parent_path());
}

TEST(PngWriter, WritesAnRgbImageThatReadsBackUnchanged) {
    // 4x3 image, three samples per pixel.
    const std::uint32_t width = 4;
    const std::uint32_t height = 3;
    std::vector<std::uint8_t> rgb(width * height * 3);
    for (std::size_t i = 0; i < rgb.size(); ++i) {
        rgb[i] = static_cast<std::uint8_t>(i * 5);
    }

    const std::filesystem::path path = scratchFile("sample_rgb.png");
    sltcd::png::writeRgb(path, rgb, width, height);

    ASSERT_TRUE(std::filesystem::exists(path));
    const LoadedPng loaded = readPng(path);
    EXPECT_EQ(loaded.width, width);
    EXPECT_EQ(loaded.height, height);
    EXPECT_EQ(loaded.bitDepth, 8);
    EXPECT_EQ(loaded.colorType, PNG_COLOR_TYPE_RGB);
    EXPECT_EQ(loaded.rgba, rgb);

    std::filesystem::remove_all(path.parent_path());
}
