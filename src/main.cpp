#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "VersionInfo.h"
#include "cli/CliOptions.h"
#include "cli/Runner.h"
#include "utils/Errors.h"
#include "utils/Logger.h"

namespace {

void printVersion() {
    std::cout << "SLTextureDecoder " << sltcd::toolVersion() << '\n'
              << "  linked OpenJPEG : " << sltcd::openjpegVersion() << '\n'
              << "  linked libpng   : " << sltcd::libpngVersion() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + (argc > 0 ? 1 : 0), argv + argc);

    sltcd::Logger logger;

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
