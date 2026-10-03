#include "utils/Logger.h"

#include <iostream>
#include <ostream>

namespace sltcd {

Logger::Logger() = default;

void Logger::info(const std::string& message) {
    write(std::cout, "INFO", message);
}

void Logger::verbose(const std::string& message) {
    if (verbose_) {
        write(std::cout, "DEBUG", message);
    }
}

void Logger::warn(const std::string& message) {
    write(std::cerr, "WARN", message);
}

void Logger::error(const std::string& message) {
    write(std::cerr, "ERROR", message);
}

void Logger::write(std::ostream& stream, const char* tag, const std::string& message) {
    stream << '[' << tag << "] " << message << '\n';
    stream.flush();
}

} // namespace sltcd
