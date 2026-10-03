#include "jpeg2000/Jpeg2000Decoder.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include <openjpeg.h>

#include "utils/Errors.h"

namespace sltcd::jpeg2000 {
namespace {

constexpr std::uint8_t kSocHigh = 0xFF;
constexpr std::uint8_t kSocLow = 0x4F;

/// Buffer size used by the OpenJPEG stream. 64 KiB keeps syscall free reads
/// large enough while staying cache friendly for the small cached textures.
constexpr OPJ_SIZE_T kStreamChunkSize = 64 * 1024;

class Diagnostics;

/// Converts an OpenJPEG image into the interleaved 8 bit layout used by the
/// rest of the tool. Declared here so decodeImpl() can call it.
std::optional<DecodedImage> convertImage(const opj_image_t& image, Diagnostics& diagnostics);

/// In-memory source handed to OpenJPEG through its stream callbacks.
struct MemoryStream {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
};

OPJ_SIZE_T readCallback(void* buffer, OPJ_SIZE_T bytes, void* userData) {
    auto* stream = static_cast<MemoryStream*>(userData);
    if (stream->offset >= stream->size) {
        return static_cast<OPJ_SIZE_T>(-1); // end of stream
    }
    const std::size_t remaining = stream->size - stream->offset;
    const std::size_t toRead = std::min(remaining, static_cast<std::size_t>(bytes));
    std::memcpy(buffer, stream->data + stream->offset, toRead);
    stream->offset += toRead;
    return static_cast<OPJ_SIZE_T>(toRead);
}

OPJ_OFF_T skipCallback(OPJ_OFF_T bytes, void* userData) {
    auto* stream = static_cast<MemoryStream*>(userData);
    if (bytes < 0) {
        const std::size_t backwards = static_cast<std::size_t>(-bytes);
        if (backwards > stream->offset) {
            return static_cast<OPJ_OFF_T>(-1);
        }
        stream->offset -= backwards;
        return bytes;
    }
    const std::size_t remaining = stream->size - stream->offset;
    const std::size_t toSkip = std::min(remaining, static_cast<std::size_t>(bytes));
    stream->offset += toSkip;
    return static_cast<OPJ_OFF_T>(toSkip);
}

OPJ_BOOL seekCallback(OPJ_OFF_T position, void* userData) {
    auto* stream = static_cast<MemoryStream*>(userData);
    if (position < 0 || static_cast<std::size_t>(position) > stream->size) {
        return OPJ_FALSE;
    }
    stream->offset = static_cast<std::size_t>(position);
    return OPJ_TRUE;
}

/// Collects the messages OpenJPEG emits through its handlers.
class Diagnostics {
public:
    void append(std::string_view message) {
        if (message.empty()) {
            return;
        }
        std::string text(message);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
            text.pop_back();
        }
        if (text.empty()) {
            return;
        }
        if (text_.size() < kMaxLength) {
            if (!text_.empty()) {
                text_ += " | ";
            }
            text_ += text;
        }
        ++count_;
    }

    const std::string& text() const noexcept { return text_; }
    std::size_t count() const noexcept { return count_; }

private:
    static constexpr std::size_t kMaxLength = 2000;
    std::string text_;
    std::size_t count_ = 0;
};

void messageHandler(const char* message, void* userData) {
    if (userData != nullptr) {
        static_cast<Diagnostics*>(userData)->append(message);
    }
}

/// RAII helpers: OpenJPEG hands out raw pointers.
class CodecGuard {
public:
    explicit CodecGuard(opj_codec_t* codec) : codec_(codec) {}
    ~CodecGuard() {
        if (codec_ != nullptr) {
            opj_destroy_codec(codec_);
        }
    }
    CodecGuard(const CodecGuard&) = delete;
    CodecGuard& operator=(const CodecGuard&) = delete;

private:
    opj_codec_t* codec_;
};

class StreamGuard {
public:
    explicit StreamGuard(opj_stream_t* stream) : stream_(stream) {}
    ~StreamGuard() {
        if (stream_ != nullptr) {
            opj_stream_destroy(stream_);
        }
    }
    StreamGuard(const StreamGuard&) = delete;
    StreamGuard& operator=(const StreamGuard&) = delete;

private:
    opj_stream_t* stream_;
};

class ImageGuard {
public:
    explicit ImageGuard(opj_image_t* image) : image_(image) {}
    ~ImageGuard() {
        if (image_ != nullptr) {
            opj_image_destroy(image_);
        }
    }
    ImageGuard(const ImageGuard&) = delete;
    ImageGuard& operator=(const ImageGuard&) = delete;

private:
    opj_image_t* image_;
};

std::string withDiagnostics(const std::string& message, const Diagnostics& diagnostics) {
    if (diagnostics.count() == 0) {
        return message;
    }
    return message + " (" + diagnostics.text() + ")";
}

/// Convert one component sample to 8 bit unsigned.
///
/// OpenJPEG hands out the raw samples: signed components are offset by half of
/// their range and components with a precision other than 8 are scaled so that
/// the most significant bits are kept. Signed components are also handled by
/// the standalone opj_decompress utility in the same way.
std::uint8_t toEightBit(std::int32_t value, const opj_image_comp_t& component) {
    const std::uint32_t precision = component.prec;
    if (component.sgnd != 0) {
        value += static_cast<std::int32_t>(1U << (precision - 1U));
    }
    if (precision > 8U) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) >> (precision - 8U));
    } else if (precision < 8U) {
        value = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << (8U - precision));
    }
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::optional<DecodedImage> decodeImpl(const std::vector<std::uint8_t>& codestream, const DecodeOptions& options,
                                       Diagnostics& diagnostics) {
    if (codestream.size() < 2 || codestream[0] != kSocHigh || codestream[1] != kSocLow) {
        diagnostics.append("the input is not a raw JPEG 2000 codestream (SOC marker missing)");
        return std::nullopt;
    }

    MemoryStream memory{codestream.data(), codestream.size(), 0};

    opj_codec_t* rawCodec = opj_create_decompress(OPJ_CODEC_J2K);
    if (rawCodec == nullptr) {
        diagnostics.append("opj_create_decompress failed");
        return std::nullopt;
    }
    CodecGuard codec(rawCodec);

    opj_set_info_handler(rawCodec, messageHandler, &diagnostics);
    opj_set_warning_handler(rawCodec, messageHandler, &diagnostics);
    opj_set_error_handler(rawCodec, messageHandler, &diagnostics);
    opj_decoder_set_strict_mode(rawCodec, options.strict ? OPJ_TRUE : OPJ_FALSE);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    if (opj_setup_decoder(rawCodec, &parameters) != OPJ_TRUE) {
        diagnostics.append("opj_setup_decoder failed");
        return std::nullopt;
    }

    opj_stream_t* rawStream = opj_stream_create(kStreamChunkSize, OPJ_TRUE /* input */);
    if (rawStream == nullptr) {
        diagnostics.append("opj_stream_create failed");
        return std::nullopt;
    }
    StreamGuard stream(rawStream);
    opj_stream_set_user_data(rawStream, &memory, nullptr);
    opj_stream_set_user_data_length(rawStream, memory.size);
    opj_stream_set_read_function(rawStream, readCallback);
    opj_stream_set_skip_function(rawStream, skipCallback);
    opj_stream_set_seek_function(rawStream, seekCallback);

    opj_image_t* rawImage = nullptr;
    if (opj_read_header(rawStream, rawCodec, &rawImage) != OPJ_TRUE || rawImage == nullptr) {
        diagnostics.append("cannot read the JPEG 2000 main header");
        return std::nullopt;
    }
    ImageGuard image(rawImage);

    if (opj_decode(rawCodec, rawStream, rawImage) != OPJ_TRUE) {
        diagnostics.append("decoding the tile data failed");
        return std::nullopt;
    }
    if (opj_end_decompress(rawCodec, rawStream) != OPJ_TRUE) {
        diagnostics.append("opj_end_decompress failed");
        return std::nullopt;
    }

    return convertImage(*rawImage, diagnostics);
}

std::optional<DecodedImage> convertImage(const opj_image_t& image, Diagnostics& diagnostics) {
    if (image.numcomps == 0) {
        diagnostics.append("the codestream declares no components");
        return std::nullopt;
    }

    const std::uint32_t width = image.x1 - image.x0;
    const std::uint32_t height = image.y1 - image.y0;
    if (width == 0 || height == 0) {
        diagnostics.append("the codestream declares an empty image area");
        return std::nullopt;
    }

    for (std::uint32_t c = 0; c < image.numcomps; ++c) {
        const opj_image_comp_t& component = image.comps[c];
        if (component.dx != 1 || component.dy != 1) {
            diagnostics.append("component " + std::to_string(c) + " is subsampled (dx=" + std::to_string(component.dx) +
                               ", dy=" + std::to_string(component.dy) + "), which is not supported");
            return std::nullopt;
        }
        if (component.w != width || component.h != height) {
            diagnostics.append("component " + std::to_string(c) + " has size " + std::to_string(component.w) + "x" +
                               std::to_string(component.h) + " but the image is " + std::to_string(width) + "x" +
                               std::to_string(height));
            return std::nullopt;
        }
        if (component.data == nullptr) {
            diagnostics.append("component " + std::to_string(c) + " carries no samples");
            return std::nullopt;
        }
    }

    DecodedImage result;
    result.width = width;
    result.height = height;
    result.components = image.numcomps;
    result.pixels.assign(static_cast<std::size_t>(width) * height * image.numcomps, 0);

    const std::size_t pixelCount = result.pixelCount();
    for (std::uint32_t c = 0; c < image.numcomps; ++c) {
        const opj_image_comp_t& component = image.comps[c];
        std::uint8_t* destination = result.pixels.data() + c;
        for (std::size_t i = 0; i < pixelCount; ++i) {
            destination[i * image.numcomps] = toEightBit(component.data[i], component);
        }
    }
    return result;
}

} // namespace

DecodedImage Jpeg2000Decoder::decode(const std::vector<std::uint8_t>& codestream, const DecodeOptions& options) {
    Diagnostics diagnostics;
    const std::optional<DecodedImage> image = decodeImpl(codestream, options, diagnostics);
    if (!image.has_value()) {
        throw DecodeError(withDiagnostics("failed to decode the JPEG 2000 codestream", diagnostics));
    }
    return *image;
}

std::optional<DecodedImage> Jpeg2000Decoder::tryDecode(const std::vector<std::uint8_t>& codestream,
                                                       const DecodeOptions& options, std::string* diagnostics) {
    Diagnostics collected;
    const std::optional<DecodedImage> image = decodeImpl(codestream, options, collected);
    if (diagnostics != nullptr) {
        *diagnostics = collected.text();
    }
    return image;
}

} // namespace sltcd::jpeg2000
