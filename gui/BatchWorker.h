#pragma once

#include <atomic>

#include <QObject>
#include <QString>

#include "batch/BatchConverter.h"

namespace sltcd::gui {

/// Runs one sltcd::batch::run() on a worker thread and mirrors it to the GUI
/// thread through queued signals.
///
/// The core logs through a Logger sink, so every run owns a private Logger; the
/// sink turns the log lines into logMessage() signals.
class BatchWorker : public QObject {
    Q_OBJECT

public:
    explicit BatchWorker(QObject* parent = nullptr);

    /// Configure the next run. Call it before process(), not while running.
    void setOptions(const batch::BatchOptions& options);
    void setVerbose(bool verbose) { verbose_ = verbose; }

public slots:
    /// Runs the batch and emits finished() (or failedToStart()) when done.
    void process();
    /// Asks the running batch to stop after the current record.
    void cancel();

signals:
    void logMessage(int level, const QString& message);
    void textureWritten(quint32 index, const QString& file, quint32 width, quint32 height, quint32 components,
                        bool complete, quint64 bytes);
    void progressChanged(quint64 records, quint64 selected, quint64 done, quint64 written, quint64 partial,
                         quint64 skipped, quint64 failed, quint64 bytes);
    void finished(quint64 records, quint64 selected, quint64 written, quint64 partial, quint64 skipped, quint64 failed,
                  quint64 bytes, bool cancelled, const QString& outDir);
    void failedToStart(const QString& message);

private:
    batch::BatchOptions options_;
    std::atomic<bool> cancelRequested_{false};
    bool verbose_ = false;
};

} // namespace sltcd::gui
