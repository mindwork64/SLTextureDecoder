#pragma once

#include <iosfwd>
#include <string>

namespace sltcd {

enum class LogLevel { Info, Verbose, Warning, Error };

/// Minimal logging facade. Everything the tool prints goes through here so that
/// the CLI can later be replaced by a GUI sink without touching the core.
class Logger {
public:
    Logger();

    void setVerbose(bool enabled) noexcept { verbose_ = enabled; }
    bool isVerbose() const noexcept { return verbose_; }

    void info(const std::string& message);
    void verbose(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    void write(std::ostream& stream, const char* tag, const std::string& message);

    bool verbose_ = false;
};

} // namespace sltcd
