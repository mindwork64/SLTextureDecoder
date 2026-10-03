#include "utils/Logger.h"

#include <iostream>
#include <ostream>

namespace sltcd {

const char* logLevelTag(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Verbose:
        return "DEBUG";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Info:
        break;
    }
    return "INFO";
}

Logger::Logger() = default;

void Logger::setSink(Sink sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = std::move(sink);
}

void Logger::clearSink() {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = Sink{};
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level == LogLevel::Verbose && !verbose_) {
        return;
    }

    Sink sink;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sink = sink_; // snapshot, so the callback runs outside the lock
    }

    if (sink) {
        sink(level, message);
        return;
    }

    writeToConsole(level, message);
}

void Logger::writeToConsole(LogLevel level, const std::string& message) {
    const char* tag = logLevelTag(level);
    std::ostream& stream = (level == LogLevel::Warning || level == LogLevel::Error) ? std::cerr : std::cout;

    std::lock_guard<std::mutex> lock(mutex_);
    stream << '[' << tag << "] " << message << '\n';
    stream.flush();
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::verbose(const std::string& message) {
    log(LogLevel::Verbose, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

} // namespace sltcd
