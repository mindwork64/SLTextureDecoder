#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "VersionInfo.h"
#include "cli/CliOptions.h"
#include "cli/Runner.h"
#include "utils/Errors.h"
#include "utils/Interrupt.h"
#include "utils/Logger.h"

namespace {

void printVersion() {
    std::cout << "SLTextureDecoder " << sltcd::toolVersion() << '\n'
              << "  linked OpenJPEG : " << sltcd::openjpegVersion() << '\n'
              << "  linked libpng   : " << sltcd::libpngVersion() << '\n'
              << "MIT licensed, educational and research use only, no warranty;\n"
                 "not affiliated with Linden Research, Inc. or the Firestorm project.\n";
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);

    sltcd::Logger logger;

    // Ctrl+C sets a flag instead of killing the process: the run then stops
    // between two records and reports the exit code of an interrupted run.
    sltcd::interrupt::installHandler();

    if (args.empty()) {
        std::cout << sltcd::cli::usageText();
        return static_cast<int>(sltcd::ErrorCode::Usage);
    }

    try {
        const sltcd::cli::CliOptions options = sltcd::cli::CliOptions::parse(args);
        logger.setVerbose(options.verbose);

        switch (options.action) {
        case sltcd::cli::CliOptions::Action::Help:
            std::cout << sltcd::cli::usageText();
            return static_cast<int>(sltcd::ErrorCode::Ok);
        case sltcd::cli::CliOptions::Action::Version:
            printVersion();
            return static_cast<int>(sltcd::ErrorCode::Ok);
        case sltcd::cli::CliOptions::Action::Decode:
            break;
        }

        const sltcd::cli::RunSummary summary = sltcd::cli::run(options, logger);
        if (summary.interrupted) {
            // The records converted so far are complete; the code tells the
            // caller that the run did not reach the end of the selection.
            return static_cast<int>(sltcd::ErrorCode::Interrupted);
        }
        return summary.failed == 0 ? static_cast<int>(sltcd::ErrorCode::Ok)
                                   : static_cast<int>(sltcd::ErrorCode::DecodeError);
    } catch (const sltcd::Error& error) {
        logger.error(error.toUserMessage());
        return static_cast<int>(error.code());
    } catch (const std::exception& error) {
        logger.error(error.what());
        return static_cast<int>(sltcd::ErrorCode::IoError);
    }
}
