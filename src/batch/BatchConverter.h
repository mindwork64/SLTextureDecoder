#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>

#include "utils/Logger.h"
#include "utils/UUID.h"

namespace sltcd::batch {

/// What to convert and how. The defaults mirror the CLI: the whole cache goes
/// into <cacheDir>/png, truncated records are decoded best effort.
struct BatchOptions {
    std::filesystem::path cacheDir;
    /// Empty means <cacheDir>/png.
    std::filesystem::path outDir;
    /// Leave out the records whose cached codestream is truncated.
    bool completeOnly = false;
    /// Rewrite PNG files that are already there.
    bool overwrite = false;
    /// Also write the assembled codestream as <uuid>.j2c next to the PNG.
    bool keepJ2k = false;
    /// Write an RGB PNG instead of RGBA (the alpha channel is dropped).
    bool noAlpha = false;
    /// Worker threads; 0 means std::thread::hardware_concurrency().
    unsigned jobs = 1;
    /// Convert at most this many textures; 0 means no limit.
    std::uint32_t limit = 0;
    /// Convert only the record at this index (ignores limit/completeOnly).
    std::optional<std::uint32_t> onlyIndex;
};

/// One PNG that was written.
struct BatchResult {
    std::uint32_t index = 0;
    UUID id;
    std::filesystem::path file;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t components = 0;
    std::uint64_t bytes = 0;
    /// False when the result came from a truncated codestream (best effort).
    bool complete = false;
};

/// Outcome of one batch run.
struct BatchSummary {
    std::filesystem::path outDir;
    std::size_t records = 0;  ///< records the cache announces
    std::size_t selected = 0; ///< records that were scheduled
    std::size_t written = 0;  ///< PNG files written
    std::size_t partial = 0;  ///< of those: best effort results
    std::size_t skipped = 0;  ///< PNG file was already present
    std::size_t failed = 0;   ///< records that could not be decoded
    std::uint64_t bytes = 0;  ///< bytes written
    bool cancelled = false;   ///< the caller asked to stop
};

/// Progress hooks. Every callback may be called from several worker threads at
/// once (and in the middle of the run), so implementations have to be
/// thread safe - a Qt signal emission or a mutex-protected counter is enough.
struct BatchCallbacks {
    /// One PNG was written. Not called for skipped records.
    std::function<void(const BatchResult&)> onResult;
    /// Snapshot of the counters after each record.
    std::function<void(const BatchSummary&)> onProgress;
    /// Polled between records; returning true stops the run.
    std::function<bool()> isCancelled;
};

/// File name of one texture: <uuid>.png or <uuid>.partial.png.
std::filesystem::path pngFile(const std::filesystem::path& outDir, const UUID& id, bool complete);

/// File name of the assembled codestream next to the PNG (keepJ2k):
/// <uuid>.j2c or <uuid>.partial.j2c.
std::filesystem::path codestreamFile(const std::filesystem::path& outDir, const UUID& id, bool complete);

/// Output directory the run will use (resolves the empty outDir default).
std::filesystem::path outputDirectory(const BatchOptions& options);

/// Convert the selection `options` describes, using `options.jobs` worker
/// threads. A broken texture is logged and counted, it never aborts the run.
/// Only problems that stop the whole run (missing cache directory, unusable
/// output directory) propagate as sltcd::Error.
BatchSummary run(const BatchOptions& options, Logger& logger, const BatchCallbacks& callbacks = BatchCallbacks{});

} // namespace sltcd::batch
