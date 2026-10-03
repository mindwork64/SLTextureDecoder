#include "cli/Runner.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cache/TextureCacheReader.h"
#include "jpeg2000/ComponentConverter.h"
#include "png/PngWriter.h"
#include "utils/Errors.h"
#include "utils/FileUtils.h"

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

RunSummary run(const CliOptions& options, Logger& logger) {
    const cache::TextureCacheReader reader = cache::TextureCacheReader::open(options.cacheDir);
    const std::filesystem::path outDir = options.outputDirectory();

    RunSummary summary;
    summary.records = reader.entries().size();
    logger.info("cache " + options.cacheDir.string() + ": " + std::to_string(summary.records) + " records, encoder " +
                reader.info().encoderVersion);

    const std::vector<std::uint32_t> indices = selectRecords(reader, options);
    summary.selected = indices.size();
    logger.info(std::to_string(summary.selected) + " texture(s) to decode into " + outDir.string());

    for (const std::uint32_t index : indices) {
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

        try {
            const cache::DecodedTexture texture = reader.decode(index);
            std::string warning;
            const std::vector<std::uint8_t> rgba = jpeg2000::toRgba(texture.image, &warning);
            png::writeRgba(target, rgba, texture.image.width, texture.image.height);

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
        }

        if (summary.written != 0 && summary.written % kProgressInterval == 0) {
            logger.info(std::to_string(summary.written) + "/" + std::to_string(summary.selected) + " written");
        }
    }

    logger.info("done: " + std::to_string(summary.written) + " PNG written (" + std::to_string(summary.partial) +
                " best effort), " + std::to_string(summary.skipped) + " skipped, " + std::to_string(summary.failed) +
                " failed");
    return summary;
}

} // namespace sltcd::cli
