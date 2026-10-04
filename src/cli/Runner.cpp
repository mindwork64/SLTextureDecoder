#include "cli/Runner.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cache/CacheLayout.h"
#include "cache/TextureCacheReader.h"
#include "jpeg2000/ComponentConverter.h"
#include "png/PngWriter.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"
#include "utils/Interrupt.h"

namespace sltcd::cli {
namespace {

/// Often enough to reassure the user on a 50k texture cache, rarely enough not
/// to flood the console.
constexpr std::size_t kProgressInterval = 250;

/// Record of `index`, with the range check that std::vector::at() lacks.
const TextureEntry& entryAt(const cache::TextureCacheReader& reader, std::uint32_t index) {
    if (index >= reader.entries().size()) {
        throw EntryNotFound("record " + std::to_string(index) + " is out of range, the cache holds " +
                            std::to_string(reader.entries().size()) + " records");
    }
    return reader.entries().at(index);
}

std::vector<std::uint32_t> selectRecords(const cache::TextureCacheReader& reader, const CliOptions& options) {
    if (options.index.has_value()) {
        return {*options.index};
    }
    if (options.id.has_value()) {
        const std::optional<std::size_t> found = reader.entries().indexOf(*options.id);
        if (!found.has_value()) {
            throw EntryNotFound("texture " + options.id->toString() + " is not in this cache");
        }
        return {static_cast<std::uint32_t>(*found)};
    }

    std::vector<std::uint32_t> indices = reader.decodableIndices(options.completeOnly);
    if (options.limit.has_value() && indices.size() > *options.limit) {
        indices.resize(*options.limit);
    }
    return indices;
}

} // namespace

std::filesystem::path outputFile(const std::filesystem::path& outDir, const UUID& id, bool complete) {
    return outDir / (id.toString() + (complete ? ".png" : ".partial.png"));
}

std::filesystem::path codestreamFile(const std::filesystem::path& outDir, const UUID& id, bool complete) {
    return outDir / (id.toString() + (complete ? ".j2c" : ".partial.j2c"));
}

RunSummary run(const CliOptions& options, Logger& logger) {
    const cache::TextureCacheReader reader = cache::TextureCacheReader::open(options.cacheDir);
    const std::filesystem::path outDir = options.outputDirectory();
    // Only used by a dry run, which needs the body file names without decoding.
    const cache::CacheLayout layout(options.cacheDir);

    RunSummary summary;
    summary.records = reader.entries().size();
    logger.info("cache " + options.cacheDir.string() + ": " + std::to_string(summary.records) + " records, encoder " +
                reader.info().encoderVersion);

    const std::vector<std::uint32_t> indices = selectRecords(reader, options);
    summary.selected = indices.size();
    logger.info(std::to_string(summary.selected) + " texture(s) to " + (options.dryRun ? "inspect" : "decode") +
                " into " + outDir.string());

    for (const std::uint32_t index : indices) {
        // Ctrl+C between two records: the record in progress is always finished,
        // so the output directory only holds complete files.
        if (interrupt::requested()) {
            summary.interrupted = true;
            logger.warn("interrupted: stopping before record " + std::to_string(index));
            break;
        }

        const TextureEntry& entry = entryAt(reader, index);
        const std::filesystem::path target = outputFile(outDir, entry.id, entry.isComplete());

        if (!entry.hasBody()) {
            ++summary.failed;
            logger.error("record " + std::to_string(index) + " (" + entry.id.toString() + ") has no body");
            continue;
        }
        if (!options.overwrite && fileutils::exists(target)) {
            ++summary.skipped;
            logger.verbose("record " + std::to_string(index) + ": " + target.filename().string() + " already exists");
            continue;
        }
        if (options.dryRun) {
            // A dry run also checks the body file, so it reports the records
            // that a real run would have to give up on instead of only
            // counting the ones texture.entries announces.
            const std::filesystem::path body = layout.bodyFile(entry.id);
            if (!fileutils::exists(body) || fileutils::size(body) != static_cast<std::uintmax_t>(entry.bodySize)) {
                ++summary.failed;
                logger.error("record " + std::to_string(index) + " (" + entry.id.toString() +
                             "): the body file is missing or has another size than the record announces");
                continue;
            }

            // Nothing is written, the counters describe the run that would
            // happen: completeness is known from the record alone.
            ++summary.written;
            if (!entry.isComplete()) {
                ++summary.partial;
            }
            logger.verbose("record " + std::to_string(index) + " (" + entry.id.toString() + ") would be written to " +
                           target.filename().string() + (entry.isComplete() ? "" : " (partial)"));
            continue;
        }

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
                fileutils::writeFile(codestreamFile(outDir, entry.id, entry.isComplete()), texture.codestream);
            }

            ++summary.written;
            if (!texture.complete) {
                ++summary.partial;
            }
            if (!warning.empty()) {
                logger.warn("record " + std::to_string(index) + ": " + warning);
            }
            logger.verbose("record " + std::to_string(index) + " (" + entry.id.toString() + ") -> " +
                           target.filename().string() + ", " + std::to_string(texture.image.width) + "x" +
                           std::to_string(texture.image.height) + ", " + std::to_string(texture.image.components) +
                           " components" + (texture.diagnostics.empty() ? "" : " [" + texture.diagnostics + "]"));
        } catch (const Error& error) {
            ++summary.failed;
            logger.error("record " + std::to_string(index) + " (" + entry.id.toString() +
                         "): " + error.toUserMessage());
        } catch (const std::exception& error) {
            // Everything else (std::bad_alloc on a hostile cache, a
            // std::filesystem_error, ...) may not end the run either.
            ++summary.failed;
            logger.error("record " + std::to_string(index) + " (" + entry.id.toString() + "): " + error.what());
        }

        if (summary.written != 0 && summary.written % kProgressInterval == 0) {
            logger.info(std::to_string(summary.written) + "/" + std::to_string(summary.selected) +
                        (options.dryRun ? " to write" : " written"));
        }
    }

    const std::string stop = summary.interrupted ? " [interrupted]" : "";
    if (options.dryRun) {
        logger.info("dry run: " + std::to_string(summary.written) + " texture(s) would be written (" +
                    std::to_string(summary.partial) + " best effort), " + std::to_string(summary.skipped) +
                    " skipped, " + std::to_string(summary.failed) + " failed" + stop);
    } else {
        logger.info("done: " + std::to_string(summary.written) + " PNG written (" + std::to_string(summary.partial) +
                    " best effort), " + std::to_string(summary.skipped) + " skipped, " +
                    std::to_string(summary.failed) + " failed" + stop);
    }
    return summary;
}

} // namespace sltcd::cli
