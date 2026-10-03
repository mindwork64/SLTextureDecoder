#pragma once

#include <cstddef>
#include <filesystem>

#include "cli/CliOptions.h"
#include "utils/Logger.h"
#include "utils/UUID.h"

namespace sltcd::cli {

/// Outcome of one conversion run.
struct RunSummary {
    std::size_t records = 0;  ///< records the cache announces
    std::size_t selected = 0; ///< records that were scheduled
    std::size_t written = 0;  ///< PNG files written
    std::size_t partial = 0;  ///< of those: best effort results
    std::size_t skipped = 0;  ///< PNG file was already present
    std::size_t failed = 0;   ///< records that could not be decoded
};

/// File name used for one texture: <uuid>.png or <uuid>.partial.png.
std::filesystem::path outputFile(const std::filesystem::path& outDir, const UUID& id, bool complete);

/// Convert the textures `options` selects. A broken texture is logged and
/// counted, it never aborts the run; only problems that stop the whole run
/// (missing cache directory, ...) propagate as sltcd::Error.
RunSummary run(const CliOptions& options, Logger& logger);

} // namespace sltcd::cli
