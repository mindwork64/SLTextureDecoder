#include "VersionInfo.h"

#include <openjpeg.h>
#include <png.h>

namespace sltcd {

std::string openjpegVersion() {
    const char* version = opj_version();
    return version != nullptr ? std::string(version) : std::string("unknown");
}

std::string libpngVersion() {
    const char* version = png_get_libpng_ver(nullptr);
    return version != nullptr ? std::string(version) : std::string("unknown");
}

std::string toolVersion() {
    return "0.1.0";
}

} // namespace sltcd
