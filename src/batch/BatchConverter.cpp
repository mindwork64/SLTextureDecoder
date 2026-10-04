#include "batch/BatchConverter.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cache/TextureCacheReader.h"
#include "cache/TextureEntries.h"
#include "jpeg2000/ComponentConverter.h"
#include "png/PngWriter.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

namespace sltcd::batch {
namespace {

/// Often enough to reassure the user on a 50k texture cache, rarely enough not
/// to flood the log.
constexpr std::size_t kProgressInterval = 250;

/// A worker thread is cheap, but OpenJPEG needs a few hundred MB per 2048x2048
/// texture, so more threads than this never pays off.
constexpr unsigned kMaxJobs = 32;

/// Record of `index`, with the range check that std::vector::at() lacks.
const TextureEntry& entryAt(const cache::TextureCacheReader& reader, std::uint32_t index) {
    if (index >= reader.entries().size()) {
        throw EntryNotFound("record " + std::to_string(index) + " is out of range, the cache holds " +
                            std::to_string(reader.entries().size()) + " records");
    }
    return reader.entries().at(index);
}

unsigned workerCount(const BatchOptions& options, std::size_t work) {
    unsigned wanted = options.jobs;
    if (wanted == 0) {
        wanted = std::thread::hardware_concurrency();
    }
    if (wanted == 0) {
        wanted = 1;
    }
    wanted = std::min(wanted, kMaxJobs);
    return static_cast<unsigned>(std::max<std::size_t>(1, std::min<std::size_t>(wanted, work)));
}

/// Log a finished record exactly like the CLI does, so both frontends read the
/// same (called with the run mutex held).
void reportSuccess(Logger& logger, std::uint32_t index, const TextureEntry& entry, const std::filesystem::path& target,
                   const cache::DecodedTexture& texture, const std::string& warning) {
    if (!warning.empty()) {
        logger.warn("record " + std::to_string(index) + ": " + warning);
    }
    logger.verbose("record " + std::to_string(index) + " (" + entry.id.toString() + ") -> " +
                   target.filename().string() + ", " + std::to_string(texture.image.width) + "x" +
                   std::to_string(texture.image.height) + ", " + std::to_string(texture.image.components) +
                   " components" + (texture.diagnostics.empty() ? "" : " [" + texture.diagnostics + "]"));
}

} // namespace

std::filesystem::path pngFile(const std::filesystem::path& outDir, const UUID& id, bool complete) {
    return outDir / (id.toString() + (complete ? ".png" : ".partial.png"));
}

std::filesystem::path codestreamFile(const std::filesystem::path& outDir, const UUID& id, bool complete) {
    return outDir / (id.toString() + (complete ? ".j2c" : ".partial.j2c"));
}

std::filesystem::path outputDirectory(const BatchOptions& options) {
    if (!options.outDir.empty()) {
        return options.outDir;
    }
    return options.cacheDir / "png";
}

BatchSummary run(const BatchOptions& options, Logger& logger, const BatchCallbacks& callbacks) {
    if (options.cacheDir.empty()) {
        throw UsageError("no cache directory given");
    }

    const cache::TextureCacheReader reader = cache::TextureCacheReader::open(options.cacheDir);

    BatchSummary summary;
    summary.outDir = outputDirectory(options);
    summary.records = reader.entries().size();

    logger.info("cache " + options.cacheDir.string() + ": " + std::to_string(summary.records) + " records, encoder " +
                reader.info().encoderVersion);

    std::vector<std::uint32_t> indices;
    if (options.onlyIndex.has_value()) {
        entryAt(reader, *options.onlyIndex); // fail before the worker threads start
        indices.push_back(*options.onlyIndex);
    } else {
        indices = reader.decodableIndices(options.completeOnly);
        if (options.limit != 0 && indices.size() > options.limit) {
            indices.resize(options.limit);
        }
    }
    summary.selected = indices.size();

    const unsigned jobs = workerCount(options, indices.size());
    logger.info(std::to_string(summary.selected) + " texture(s) to decode into " + summary.outDir.string() + " using " +
                std::to_string(jobs) + " worker thread(s)");

    if (indices.empty()) {
        logger.info("done: nothing to do");
        return summary;
    }

    {
        // Fail fast instead of reporting tens of thousands of identical write
        // errors one by one.
        std::error_code ec;
        std::filesystem::create_directories(summary.outDir, ec);
        if (ec) {
            throw WriteError("cannot create output directory '" + summary.outDir.string() + "': " + ec.message());
        }
    }

    std::atomic<std::size_t> next{0};
    std::atomic<bool> cancelled{false};
    std::mutex mutex; // guards the counters and keeps log lines whole

    const auto worker = [&]() {
        for (;;) {
            const std::size_t slot = next.fetch_add(1);
            if (slot >= indices.size()) {
                break;
            }
            if (callbacks.isCancelled && callbacks.isCancelled()) {
                cancelled = true;
                break;
            }

            const std::uint32_t index = indices[slot];
            const TextureEntry& entry = entryAt(reader, index);
            const std::filesystem::path target = pngFile(summary.outDir, entry.id, entry.isComplete());

            if (!entry.hasBody()) {
                std::lock_guard<std::mutex> lock(mutex);
                ++summary.failed;
                logger.error("record " + std::to_string(index) + " (" + entry.id.toString() + ") has no body");
                continue;
            }
            if (!options.overwrite && fileutils::exists(target)) {
                std::lock_guard<std::mutex> lock(mutex);
                ++summary.skipped;
                logger.verbose("record " + std::to_string(index) + ": " + target.filename().string() +
                               " already exists");
                continue;
            }

            BatchResult result;
            result.index = index;
            result.id = entry.id;
            result.file = target;

            try {
                const cache::DecodedTexture texture = reader.decode(index, options.keepJ2k);
                std::string warning;
                if (options.noAlpha) {
                    png::writeRgb(target, jpeg2000::toRgb(texture.image, &warning), texture.image.width,
                                  texture.image.height);
                } else {
                    png::writeRgba(target, jpeg2000::toRgba(texture.image, &warning), texture.image.width,
                                   texture.image.height);
                }
                if (options.keepJ2k) {
                    fileutils::writeFile(codestreamFile(summary.outDir, entry.id, entry.isComplete()),
                                         texture.codestream);
                }

                result.width = texture.image.width;
                result.height = texture.image.height;
                result.components = texture.image.components;
                result.complete = texture.complete;
                result.bytes = fileutils::size(target);

                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ++summary.written;
                    summary.bytes += result.bytes;
                    if (!texture.complete) {
                        ++summary.partial;
                    }
                    reportSuccess(logger, index, entry, target, texture, warning);
                    if (summary.written % kProgressInterval == 0) {
                        logger.info(std::to_string(summary.written) + "/" + std::to_string(summary.selected) +
                                    " written");
                    }
                }
                if (callbacks.onResult) {
                    callbacks.onResult(result);
                }
            } catch (const Error& error) {
                std::lock_guard<std::mutex> lock(mutex);
                ++summary.failed;
                logger.error("record " + std::to_string(index) + " (" + entry.id.toString() +
                             "): " + error.toUserMessage());
            } catch (const std::exception& error) {
                std::lock_guard<std::mutex> lock(mutex);
                ++summary.failed;
                logger.error("record " + std::to_string(index) + " (" + entry.id.toString() + "): " + error.what());
            }

            if (callbacks.onProgress) {
                std::lock_guard<std::mutex> lock(mutex);
                callbacks.onProgress(summary);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(jobs - 1);
    for (unsigned i = 1; i < jobs; ++i) {
        threads.emplace_back(worker);
    }
    worker(); // the calling thread works too, so jobs == 1 stays single threaded
    for (std::thread& thread : threads) {
        thread.join();
    }

    summary.cancelled = cancelled.load();
    logger.info("done: " + std::to_string(summary.written) + " PNG written (" + std::to_string(summary.partial) +
                " best effort), " + std::to_string(summary.skipped) + " skipped, " + std::to_string(summary.failed) +
                " failed" + (summary.cancelled ? " [cancelled]" : ""));
    return summary;
}

} // namespace sltcd::batch
