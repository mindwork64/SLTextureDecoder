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
    std::size_t written = 0;  ///< PNG files written (see the note on dry runs)
    std::size_t partial = 0;  ///< of those: best effort results
    std::size_t skipped = 0;  ///< PNG file was already present
    std::size_t failed = 0;   ///< records that could not be decoded
    /// True when the run stopped early because the user interrupted it; see
    /// sltcd::interrupt. The records converted before that are complete.
    bool interrupted = false;
};

/// File name used for one texture: <uuid>.png or <uuid>.partial.png.
std::filesystem::path outputFile(const std::filesystem::path& outDir, const UUID& id, bool complete);

/// File name of the assembled codestream next to the PNG (--keep-j2k):
/// <uuid>.j2c or <uuid>.partial.j2c.
std::filesystem::path codestreamFile(const std::filesystem::path& outDir, const UUID& id, bool complete);

/// Convert the textures `options` selects. A broken texture is logged and
/// counted, it never aborts the run; only problems that stop the whole run
/// (missing cache directory, ...) propagate as sltcd::Error.
///
/// A record that fails with an unexpected std::exception (std::bad_alloc on a
/// huge texture, a std::filesystem_error, ...) counts as failed as well, so one
/// broken record can never end the whole run.
///
/// With options.dryRun nothing is written and no directory is created: the
/// counters then describe what a real run would produce, and summary.written
/// counts the records that would be converted.
///
/// Returns early with summary.interrupted set when sltcd::interrupt::requested()
/// became true (Ctrl+C) between two records.
RunSummary run(const CliOptions& options, Logger& logger);

} // namespace sltcd::cli
