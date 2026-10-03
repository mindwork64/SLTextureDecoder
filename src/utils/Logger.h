#pragma once

#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>

namespace sltcd {

enum class LogLevel { Info, Verbose, Warning, Error };

/// Bare tag of a level ("INFO", "DEBUG", "WARN", "ERROR"); the GUI and any
/// other sink can render it without repeating the mapping.
const char* logLevelTag(LogLevel level) noexcept;

/// Minimal logging facade. Everything the tool prints goes through here so that
/// the CLI can be replaced by a GUI sink without touching the core.
///
/// All methods are safe to call from several threads at once (the batch
/// converter logs from its worker threads). A sink must not log back into the
/// same logger.
class Logger {
public:
    /// Consumer of the log lines. When one is installed the console output is
    /// suppressed and every accepted line is handed to the sink instead.
    using Sink = std::function<void(LogLevel, const std::string&)>;

    Logger();

    void setSink(Sink sink);
    void clearSink();

    void setVerbose(bool enabled) noexcept { verbose_ = enabled; }
    bool isVerbose() const noexcept { return verbose_; }

    /// Emit `message` at `level`; Verbose lines are dropped unless enabled.
    void log(LogLevel level, const std::string& message);

    void info(const std::string& message);
    void verbose(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    void writeToConsole(LogLevel level, const std::string& message);

    std::mutex mutex_;
    Sink sink_;
    bool verbose_ = false;
};

} // namespace sltcd
