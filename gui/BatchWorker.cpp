#include "BatchWorker.h"

#include <exception>
#include <string>

#include "utils/Errors.h"
#include "utils/Logger.h"

namespace sltcd::gui {
namespace {

/// std::filesystem::path -> QString without an encoding detour, so non ASCII
/// cache directories survive.
QString fromPath(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}

} // namespace

BatchWorker::BatchWorker(QObject* parent) : QObject(parent) {}

void BatchWorker::setOptions(const batch::BatchOptions& options) {
    options_ = options;
}

void BatchWorker::cancel() {
    cancelRequested_.store(true);
}

void BatchWorker::process() {
    cancelRequested_.store(false);

    Logger logger;
    logger.setVerbose(verbose_);
    logger.setSink([this](LogLevel level, const std::string& message) {
        emit logMessage(static_cast<int>(level), QString::fromStdString(message));
    });

    batch::BatchCallbacks callbacks;
    callbacks.isCancelled = [this]() { return cancelRequested_.load(); };
    callbacks.onResult = [this](const batch::BatchResult& result) {
        emit textureWritten(result.index, fromPath(result.file), result.width, result.height, result.components,
                            result.complete, result.bytes);
    };
    callbacks.onProgress = [this](const batch::BatchSummary& summary) {
        const quint64 done = summary.written + summary.skipped + summary.failed;
        emit progressChanged(summary.records, summary.selected, done, summary.written, summary.partial, summary.skipped,
                             summary.failed, summary.bytes);
    };

    try {
        const batch::BatchSummary summary = batch::run(options_, logger, callbacks);
        emit finished(summary.records, summary.selected, summary.written, summary.partial, summary.skipped,
                      summary.failed, summary.bytes, summary.cancelled, fromPath(summary.outDir));
    } catch (const Error& error) {
        emit failedToStart(QString::fromStdString(error.toUserMessage()));
    } catch (const std::exception& error) {
        emit failedToStart(QString::fromStdString(error.what()));
    }

    logger.clearSink();
}

} // namespace sltcd::gui
