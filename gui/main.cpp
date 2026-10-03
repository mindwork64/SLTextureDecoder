#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QString>

#include "MainWindow.h"
#include "VersionInfo.h"
#include "batch/BatchConverter.h"
#include "cache/TextureCacheReader.h"
#include "cli/CliOptions.h"
#include "utils/Errors.h"
#include "utils/Logger.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

/// A windowed build owns no console, so the headless mode borrows the console of
/// the shell it was started from - but only when stdout does not already lead
/// somewhere (a file or a pipe), so `--batch > log.txt` keeps working.
void attachParentConsole() {
#ifdef _WIN32
    const HANDLE current = GetStdHandle(STD_OUTPUT_HANDLE);
    if (current != nullptr && current != INVALID_HANDLE_VALUE) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) {
        return;
    }
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
#endif
}

unsigned parseUnsigned(const std::string& text) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        throw sltcd::UsageError("expected a number, got '" + text + "'");
    }
    return static_cast<unsigned>(std::stoul(text));
}

/// Pulls "--jobs <n>" / "--jobs=<n>" out of `args` and drops "--batch"; the
/// rest is left to the command line parser of the CLI, so both frontends accept
/// the same options.
std::vector<std::string> extractJobs(const std::vector<std::string>& args, unsigned& jobs) {
    constexpr const char* kPrefix = "--jobs=";
    const std::size_t prefixLength = std::char_traits<char>::length(kPrefix);

    std::vector<std::string> rest;
    rest.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--batch") {
            continue;
        }
        if (arg == "--jobs") {
            if (i + 1 >= args.size()) {
                throw sltcd::UsageError("--jobs needs the number of worker threads");
            }
            jobs = parseUnsigned(args[++i]);
            continue;
        }
        if (arg.rfind(kPrefix, 0) == 0) {
            jobs = parseUnsigned(arg.substr(prefixLength));
            continue;
        }
        rest.push_back(arg);
    }
    return rest;
}

int runHeadless(const std::vector<std::string>& args, sltcd::Logger& logger) {
    unsigned jobs = 0;
    const std::vector<std::string> rest = extractJobs(args, jobs);
    const sltcd::cli::CliOptions options = sltcd::cli::CliOptions::parse(rest);
    logger.setVerbose(options.verbose);

    switch (options.action) {
    case sltcd::cli::CliOptions::Action::Help:
        std::cout << sltcd::cli::usageText() << '\n'
                  << "SLTextureDecoderGUI only:\n"
                     "  --jobs <n>   worker threads (0 = one per logical CPU)\n"
                     "  --batch      convert without opening a window\n";
        return static_cast<int>(sltcd::ErrorCode::Ok);
    case sltcd::cli::CliOptions::Action::Version:
        std::cout << "SLTextureDecoderGUI " << sltcd::toolVersion() << '\n'
                  << "MIT licensed, educational and research use only, no warranty;\n"
                     "not affiliated with Linden Research, Inc. or the Firestorm project.\n";
        return static_cast<int>(sltcd::ErrorCode::Ok);
    case sltcd::cli::CliOptions::Action::Decode:
        break;
    }

    sltcd::batch::BatchOptions batch;
    batch.cacheDir = options.cacheDir;
    batch.outDir = options.outDir;
    batch.completeOnly = options.completeOnly;
    batch.overwrite = options.overwrite;
    batch.jobs = jobs;
    batch.limit = options.limit.value_or(0);
    if (options.index.has_value()) {
        batch.onlyIndex = options.index;
    } else if (options.id.has_value()) {
        const sltcd::cache::TextureCacheReader reader = sltcd::cache::TextureCacheReader::open(options.cacheDir);
        const std::optional<std::size_t> found = reader.entries().indexOf(*options.id);
        if (!found.has_value()) {
            throw sltcd::EntryNotFound("texture " + options.id->toString() + " is not in this cache");
        }
        batch.onlyIndex = static_cast<std::uint32_t>(*found);
    }

    const sltcd::batch::BatchSummary summary = sltcd::batch::run(batch, logger);
    return summary.failed == 0 ? static_cast<int>(sltcd::ErrorCode::Ok)
                               : static_cast<int>(sltcd::ErrorCode::DecodeError);
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    const bool headless = std::find(args.begin(), args.end(), std::string("--batch")) != args.end();

    if (headless) {
        attachParentConsole();
        sltcd::Logger logger;
        try {
            return runHeadless(args, logger);
        } catch (const sltcd::Error& error) {
            logger.error(error.toUserMessage());
            return static_cast<int>(error.code());
        } catch (const std::exception& error) {
            logger.error(error.what());
            return static_cast<int>(sltcd::ErrorCode::IoError);
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SLTextureDecoderGUI"));
    QApplication::setOrganizationName(QStringLiteral("SLTextureDecoder"));
    QApplication::setApplicationVersion(QString::fromStdString(sltcd::toolVersion()));

    sltcd::gui::MainWindow window;
    window.show();
    return QApplication::exec();
}
