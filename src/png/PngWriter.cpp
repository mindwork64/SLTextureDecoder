#include "png/PngWriter.h"

#include <png.h>

#include <csetjmp>
#include <cstdio>
#include <string>
#include <system_error>

#include "utils/Errors.h"

namespace sltcd::png {
namespace {

constexpr std::size_t kChannels = 4;
constexpr int kBitDepth = 8;

/// libpng reports errors through longjmp; stash the text for the exception.
void onError(png_structp png, png_const_charp message) {
    auto* text = static_cast<std::string*>(png_get_error_ptr(png));
    if (text != nullptr) {
        *text = message != nullptr ? message : "unknown libpng error";
    }
    longjmp(png_jmpbuf(png), 1);
}

void onWarning(png_structp /*png*/, png_const_charp /*message*/) {
    // Warnings are not fatal and nothing here needs the extra detail.
}

} // namespace

void writeRgba(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba, std::uint32_t width,
               std::uint32_t height) {
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels;
    if (width == 0 || height == 0 || rgba.size() != expected) {
        throw WriteError("refusing to write " + path.string() + ": " + std::to_string(rgba.size()) + " bytes hold no " +
                         std::to_string(width) + "x" + std::to_string(height) + " RGBA image");
    }

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw WriteError("cannot create directory '" + path.parent_path().string() + "': " + ec.message());
        }
    }

    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) {
        throw WriteError("cannot open file for writing: " + path.string());
    }

    std::string error;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &error, onError, onWarning);
    if (png == nullptr) {
        std::fclose(file);
        throw WriteError("libpng is out of memory while writing " + path.string());
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        throw WriteError("libpng is out of memory while writing " + path.string());
    }

    if (setjmp(png_jmpbuf(png)) != 0) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        std::error_code ec;
        std::filesystem::remove(path, ec);
        throw WriteError("failed to write " + path.string() + ": " + error);
    }

    png_init_io(png, file);
    png_set_IHDR(png, info, width, height, kBitDepth, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    // libpng wants one pointer per row; the pixel buffer is already row major,
    // so fill a vector of row starts instead of copying the pixels.
    std::vector<png_bytep> rows(height);
    for (std::uint32_t y = 0; y < height; ++y) {
        rows[y] = const_cast<png_bytep>(rgba.data() + static_cast<std::size_t>(y) * width * kChannels);
    }
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);

    if (std::fclose(file) != 0) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        throw WriteError("failed to flush " + path.string());
    }
}

} // namespace sltcd::png
