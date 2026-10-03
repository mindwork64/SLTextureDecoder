#pragma once

#include <QMainWindow>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QThread;

namespace sltcd::gui {

class BatchWorker;

/// Main window of the desktop frontend: pick the texture cache, pick where the
/// PNG files go, convert and watch what happens.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void chooseCacheDir();
    void chooseOutputDir();
    void startBatch();
    void cancelBatch();
    void openOutputDir();
    void clearLog();
    void onCacheDirEdited();
    void onOutputDirEdited();
    void onLogMessage(int level, const QString& message);
    void onTextureWritten(quint32 index, const QString& file, quint32 width, quint32 height, quint32 components,
                          bool complete, quint64 bytes);
    void onProgress(quint64 records, quint64 selected, quint64 done, quint64 written, quint64 partial, quint64 skipped,
                    quint64 failed, quint64 bytes);
    void onFinished(quint64 records, quint64 selected, quint64 written, quint64 partial, quint64 skipped,
                    quint64 failed, quint64 bytes, bool cancelled, const QString& outDir);
    void onFailedToStart(const QString& message);
    void onResultSelected();

private:
    void buildUi();
    void loadSettings();
    void saveSettings() const;
    void setRunning(bool running);
    void appendLog(const QString& line, const QString& color);
    void showCacheInfo();
    /// Quits the helper thread, waits for it and deletes thread and worker.
    void stopWorker();
    QString cacheDir() const;
    QString outputDir() const;

    QLineEdit* cacheDirEdit_ = nullptr;
    QPushButton* cacheBrowseButton_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QPushButton* outputBrowseButton_ = nullptr;
    QCheckBox* completeOnlyCheck_ = nullptr;
    QCheckBox* overwriteCheck_ = nullptr;
    QCheckBox* verboseCheck_ = nullptr;
    QSpinBox* jobsSpin_ = nullptr;
    QSpinBox* limitSpin_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* openOutputButton_ = nullptr;
    QPushButton* clearLogButton_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* cacheInfoLabel_ = nullptr;
    QPlainTextEdit* logView_ = nullptr;
    QListWidget* resultList_ = nullptr;
    QLabel* previewLabel_ = nullptr;

    QThread* workerThread_ = nullptr;
    BatchWorker* worker_ = nullptr;

    QString lastOutDir_;
    QString inspectedDir_;
    int resultItems_ = 0;
};

} // namespace sltcd::gui
