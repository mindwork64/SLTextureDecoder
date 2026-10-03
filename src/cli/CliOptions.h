#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "utils/UUID.h"

namespace sltcd::cli {

/// A validated command line.
struct CliOptions {
    enum class Action { Help, Version, Decode };

    Action action = Action::Help;
    /// Texture cache directory (texture.entries + texture.cache + shards).
    std::filesystem::path cacheDir;
    /// Where the PNG files go; empty means <cacheDir>/png.
    std::filesystem::path outDir;
    /// Decode one texture instead of the whole cache.
    std::optional<UUID> id;
    /// Decode only the record at this index.
    std::optional<std::uint32_t> index;
    /// Decode at most this many textures.
    std::optional<std::uint32_t> limit;
    /// Skip the records whose cached codestream is truncated.
    bool completeOnly = false;
    /// Rewrite PNG files that are already there.
    bool overwrite = false;
    bool verbose = false;

    /// Throws sltcd::UsageError for an unusable command line.
    static CliOptions parse(const std::vector<std::string>& args);

    std::filesystem::path outputDirectory() const;
};

/// Help text of the tool, used for "--help" and for a bare invocation.
std::string usageText();

} // namespace sltcd::cli
