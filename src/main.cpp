#include <iostream>
#include <string>
#include <vector>

#include "VersionInfo.h"
#include "utils/Constants.h"
#include "utils/Errors.h"

namespace {

void printVersion() {
    std::cout << "SLTextureDecoder " << sltcd::toolVersion() << '\n'
              << "  linked OpenJPEG : " << sltcd::openjpegVersion() << '\n'
              << "  linked libpng   : " << sltcd::libpngVersion() << '\n';
}

void printUsage() {
    std::cout << "SLTextureDecoder " << sltcd::toolVersion() << '\n'
              << "Decodes Second Life / Firestorm JPEG2000 texture caches into PNG.\n\n"
              << "Usage:\n"
              << "  SLTextureDecoder --version        show version and linked libraries\n"
              << "  SLTextureDecoder --help           show this help\n"
              << "  SLTextureDecoder --cache-dir <dir>  decode a texture cache directory\n\n"
              << "Format constants:\n"
              << "  texture header size : " << sltcd::CacheFormatConfig::kTextureHeaderSize << " bytes\n"
              << "  entries info size   : " << sltcd::CacheFormatConfig::kEntriesInfoSize << " bytes\n"
              << "  entry record size   : " << sltcd::CacheFormatConfig::kEntrySizeBytes << " bytes\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);

    if (args.empty()) {
        printUsage();
        return static_cast<int>(sltcd::ErrorCode::Usage);
    }

    if (args[0] == "--version" || args[0] == "-V") {
        printVersion();
        return static_cast<int>(sltcd::ErrorCode::Ok);
    }

    if (args[0] == "--help" || args[0] == "-h") {
        printUsage();
        return static_cast<int>(sltcd::ErrorCode::Ok);
    }

    std::cerr << "error: cache decoding is not implemented yet (project skeleton)\n";
    return static_cast<int>(sltcd::ErrorCode::Usage);
}
