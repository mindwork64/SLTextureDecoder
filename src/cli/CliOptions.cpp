#include "cli/CliOptions.h"

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <system_error>

#include "VersionInfo.h"
#include "utils/Constants.h"
#include "utils/Errors.h"

namespace sltcd::cli {
namespace {

std::uint32_t parseCount(const std::string& option, const std::string& text) {
    std::uint32_t value = 0;
    const char* end = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        throw UsageError(option + " expects a number, got '" + text + "'");
    }
    return value;
}

} // namespace

CliOptions CliOptions::parse(const std::vector<std::string>& args) {
    CliOptions options;
    bool haveCacheDir = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        /// Consumes the value that belongs to `arg`.
        const auto value = [&args, &i, &arg]() -> std::string {
            if (i + 1 >= args.size()) {
                throw UsageError("option " + arg + " expects a value");
            }
            return args[++i];
        };

        if (arg == "--help" || arg == "-h") {
            options.action = Action::Help;
            return options;
        }
        if (arg == "--version" || arg == "-V") {
            options.action = Action::Version;
            return options;
        }
        if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--complete-only") {
            options.completeOnly = true;
        } else if (arg == "--overwrite") {
            options.overwrite = true;
        } else if (arg == "--keep-j2k") {
            options.keepJ2k = true;
        } else if (arg == "--no-alpha") {
            options.noAlpha = true;
        } else if (arg == "--cache-dir") {
            options.cacheDir = value();
            haveCacheDir = true;
        } else if (arg == "--out-dir") {
            options.outDir = value();
        } else if (arg == "--id") {
            try {
                options.id = UUID::parse(value());
            } catch (const std::invalid_argument&) {
                throw UsageError("--id expects a UUID such as 8709e46e-1e81-1e6c-9d32-4bd7d3e2ba4b");
            }
        } else if (arg == "--index") {
            options.index = parseCount(arg, value());
        } else if (arg == "--limit") {
            options.limit = parseCount(arg, value());
        } else {
            throw UsageError("unknown option: " + arg);
        }
    }

    if (!haveCacheDir) {
        throw UsageError("--cache-dir is required");
    }
    options.action = Action::Decode;
    return options;
}

std::filesystem::path CliOptions::outputDirectory() const {
    return outDir.empty() ? cacheDir / "png" : outDir;
}

std::string usageText() {
    return "SLTextureDecoder " + toolVersion() +
           "\nDecodes Second Life / Firestorm JPEG 2000 texture caches into PNG.\n\n"
           "Usage:\n"
           "  SLTextureDecoder --cache-dir <dir> [options]\n\n"
           "Options:\n"
           "  --cache-dir <dir>   texture cache directory to read (required)\n"
           "  --out-dir <dir>     where the PNG files go (default: <cache-dir>/png)\n"
           "  --id <uuid>         decode one texture instead of the whole cache\n"
           "  --index <n>         decode only the record at this index\n"
           "  --limit <n>         stop after n textures\n"
           "  --complete-only     skip records whose cached codestream is truncated\n"
           "  --overwrite         write over existing PNG files\n"
           "  --keep-j2k          also write the assembled codestream next to the PNG\n"
           "  --no-alpha          write an RGB PNG instead of RGBA\n"
           "  -v, --verbose       log every record\n"
           "  -h, --help          show this help\n"
           "  -V, --version       show version and linked libraries\n\n"
           "Output:\n"
           "  <uuid>.png          decoded from a complete codestream\n"
           "  <uuid>.partial.png  best effort result from a truncated codestream\n"
           "  <uuid>.j2c          assembled codestream, only with --keep-j2k\n\n"
           "Format constants:\n"
           "  texture header size : " +
           std::to_string(CacheFormatConfig::kTextureHeaderSize) + " bytes\n" +
           "  entries info size   : " + std::to_string(CacheFormatConfig::kEntriesInfoSize) + " bytes\n" +
           "  entry record size   : " + std::to_string(CacheFormatConfig::kEntrySizeBytes) + " bytes\n\n" +
           "Notice:\n"
           "  Educational and research use only; the cache is opened read-only and no\n"
           "  network access is performed. Not affiliated with Linden Research, Inc.\n"
           "  (Second Life) or the Phoenix Firestorm Project (Firestorm Viewer), whose\n"
           "  names are used descriptively. Copyright in the decoded textures stays\n"
           "  with their creators - see NOTICE.md. Use your own cache only.\n";
}

} // namespace sltcd::cli
