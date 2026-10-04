#include "png/PngWriter.h"

#include <png.h>

#include <csetjmp>
#include <cstdio>
#include <string>
#include <system_error>

#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace sltcd::png {
namespace {

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

/// Shared body of writeRgba/writeRgb; `channels` samples per pixel and
/// `colorType` the matching libpng colour type.
void writePixels(const std::filesystem::path& path, const std::vector<std::uint8_t>& pixels, std::uint32_t width,
                 std::uint32_t height, std::size_t channels, int colorType) {
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels;
    if (width == 0 || height == 0 || pixels.size() != expected) {
        throw WriteError("refusing to write " + path.string() + ": " + std::to_string(pixels.size()) +
                         " bytes hold no " + std::to_string(width) + "x" + std::to_string(height) + " image with " +
                         std::to_string(channels) + " channels");
    }

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw WriteError("cannot create directory '" + path.parent_path().string() + "': " + ec.message());
        }
    }

    // libpng writes through a FILE*, so the atomic write of fileutils cannot be
    // used here; the pattern is the same: into "<path>.part", then rename it
    // over the target. An interrupted run leaves a stale .part file - which no
    // later run mistakes for a finished PNG - instead of a broken PNG.
    const std::filesystem::path temporary = fileutils::temporaryPath(path);

    std::FILE* file = std::fopen(temporary.string().c_str(), "wb");
    if (file == nullptr) {
        throw WriteError("cannot open file for writing: " + temporary.string());
    }

    std::string error;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &error, onError, onWarning);
    if (png == nullptr) {
        std::fclose(file);
        fileutils::removeFile(temporary);
        throw WriteError("libpng is out of memory while writing " + path.string());
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        fileutils::removeFile(temporary);
        throw WriteError("libpng is out of memory while writing " + path.string());
    }

    if (setjmp(png_jmpbuf(png)) != 0) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        fileutils::removeFile(temporary);
        throw WriteError("failed to write " + path.string() + ": " + error);
    }

    png_init_io(png, file);
    png_set_IHDR(png, info, width, height, kBitDepth, colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    // libpng wants one pointer per row; the pixel buffer is already row major,
    // so fill a vector of row starts instead of copying the pixels.
    std::vector<png_bytep> rows(height);
    for (std::uint32_t y = 0; y < height; ++y) {
        rows[y] = const_cast<png_bytep>(pixels.data() + static_cast<std::size_t>(y) * width * channels);
    }
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);

    if (std::fclose(file) != 0) {
        fileutils::removeFile(temporary);
        throw WriteError("failed to flush " + path.string());
    }

    fileutils::replaceFile(temporary, path);
}

} // namespace

void writeRgba(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba, std::uint32_t width,
               std::uint32_t height) {
    writePixels(path, rgba, width, height, 4, PNG_COLOR_TYPE_RGB_ALPHA);
}

void writeRgb(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgb, std::uint32_t width,
              std::uint32_t height) {
    writePixels(path, rgb, width, height, 3, PNG_COLOR_TYPE_RGB);
}

} // namespace sltcd::png
