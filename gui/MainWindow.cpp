#include "MainWindow.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <thread>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSize>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "BatchWorker.h"
#include "VersionInfo.h"
#include "cache/TextureCacheReader.h"
#include "utils/Errors.h"
#include "utils/Logger.h"

namespace sltcd::gui {
namespace {

/// Results listed in the file view. A full cache holds tens of thousands of
/// textures, listing all of them would only make the window slow.
constexpr int kMaxListedResults = 2000;

/// Longest edge of the preview pane.
constexpr int kPreviewSize = 512;

constexpr const char* kInfoColor = "#d8d8d8";
constexpr const char* kVerboseColor = "#8c8c8c";
constexpr const char* kHintColor = "#7fb0ff";
constexpr const char* kWarningColor = "#e0a030";
constexpr const char* kErrorColor = "#ff6b6b";

QString formatBytes(quint64 bytes) {
    static const char* units[] = {"B", "kB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : 1) + QStringLiteral(" ") + QString::fromLatin1(units[unit]);
}

std::filesystem::path toPath(const QString& text) {
    return std::filesystem::path(text.toStdWString());
}

QString fromPath(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SL Texture Decoder %1").arg(QString::fromStdString(toolVersion())));
    buildUi();
    loadSettings();
    statusBar()->showMessage(tr("Ready"));
}

MainWindow::~MainWindow() {
    if (workerThread_ == nullptr) {
        return;
    }

    worker_->cancel();
    workerThread_->quit();
    if (!workerThread_->wait(15000)) {
        // The window is going away, so the only remaining option is to stop the
        // helper thread hard; the process exits right after.
        workerThread_->terminate();
        workerThread_->wait(1000);
    }

    delete worker_;
    delete workerThread_;
    worker_ = nullptr;
    workerThread_ = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    if (workerThread_ != nullptr) {
        worker_->cancel();
        workerThread_->quit();
        if (!workerThread_->wait(15000)) {
            QMessageBox::information(this, tr("Still converting"),
                                     tr("The current texture is still being decoded, please close the window "
                                        "again in a moment."));
            event->ignore();
            return;
        }
        delete worker_;
        delete workerThread_;
        worker_ = nullptr;
        workerThread_ = nullptr;
    }
    event->accept();
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // --- paths ---------------------------------------------------------------
    auto* pathsGroup = new QGroupBox(tr("Cache and output"), central);
    auto* pathsForm = new QFormLayout(pathsGroup);

    auto* cacheRow = new QWidget(pathsGroup);
    auto* cacheRowLayout = new QHBoxLayout(cacheRow);
    cacheRowLayout->setContentsMargins(0, 0, 0, 0);
    cacheDirEdit_ = new QLineEdit(cacheRow);
    cacheDirEdit_->setPlaceholderText(tr("folder holding texture.entries, texture.cache and the 0..f shards"));
    cacheDirEdit_->setToolTip(tr("The Second Life / Firestorm texture cache, usually "
                                 "<user>\\AppData\\Roaming\\SecondLife_x64\\texturecache"));
    cacheBrowseButton_ = new QPushButton(tr("Browse..."), cacheRow);
    cacheRowLayout->addWidget(cacheDirEdit_, 1);
    cacheRowLayout->addWidget(cacheBrowseButton_);
    pathsForm->addRow(tr("Cache folder"), cacheRow);

    auto* outputRow = new QWidget(pathsGroup);
    auto* outputRowLayout = new QHBoxLayout(outputRow);
    outputRowLayout->setContentsMargins(0, 0, 0, 0);
    outputDirEdit_ = new QLineEdit(outputRow);
    outputDirEdit_->setPlaceholderText(tr("where the PNG files go; empty means <cache>/png"));
    outputBrowseButton_ = new QPushButton(tr("Browse..."), outputRow);
    outputRowLayout->addWidget(outputDirEdit_, 1);
    outputRowLayout->addWidget(outputBrowseButton_);
    pathsForm->addRow(tr("Output folder"), outputRow);

    cacheInfoLabel_ = new QLabel(tr("Pick the cache folder to see what it holds."), pathsGroup);
    cacheInfoLabel_->setWordWrap(true);
    pathsForm->addRow(QString(), cacheInfoLabel_);
    root->addWidget(pathsGroup);

    // --- options -------------------------------------------------------------
    auto* optionsGroup = new QGroupBox(tr("Options"), central);
    auto* optionsLayout = new QHBoxLayout(optionsGroup);
    completeOnlyCheck_ = new QCheckBox(tr("Complete records only"), optionsGroup);
    completeOnlyCheck_->setToolTip(tr("Decode only the textures whose cached codestream is complete."));
    overwriteCheck_ = new QCheckBox(tr("Overwrite existing files"), optionsGroup);
    overwriteCheck_->setToolTip(tr("Without this, textures that already have a PNG are skipped, so an interrupted "
                                   "run can be resumed by starting it again."));
    verboseCheck_ = new QCheckBox(tr("Verbose log"), optionsGroup);
    verboseCheck_->setToolTip(tr("Log every texture, not just warnings and errors."));
    jobsSpin_ = new QSpinBox(optionsGroup);
    jobsSpin_->setRange(0, 32);
    jobsSpin_->setSpecialValueText(tr("auto"));
    jobsSpin_->setValue(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
    jobsSpin_->setToolTip(tr("Worker threads; auto uses the number of logical CPUs."));
    limitSpin_ = new QSpinBox(optionsGroup);
    limitSpin_->setRange(0, 1000000);
    limitSpin_->setSpecialValueText(tr("all"));
    limitSpin_->setToolTip(tr("Convert at most this many textures, in cache order."));
    optionsLayout->addWidget(completeOnlyCheck_);
    optionsLayout->addWidget(overwriteCheck_);
    optionsLayout->addWidget(verboseCheck_);
    optionsLayout->addStretch(1);
    optionsLayout->addWidget(new QLabel(tr("Threads:"), optionsGroup));
    optionsLayout->addWidget(jobsSpin_);
    optionsLayout->addWidget(new QLabel(tr("Limit:"), optionsGroup));
    optionsLayout->addWidget(limitSpin_);
    root->addWidget(optionsGroup);

    // --- actions -------------------------------------------------------------
    auto* actionRow = new QHBoxLayout();
    startButton_ = new QPushButton(tr("Start conversion"), central);
    startButton_->setDefault(true);
    cancelButton_ = new QPushButton(tr("Cancel"), central);
    cancelButton_->setEnabled(false);
    openOutputButton_ = new QPushButton(tr("Open output folder"), central);
    openOutputButton_->setEnabled(false);
    clearLogButton_ = new QPushButton(tr("Clear log"), central);
    actionRow->addWidget(startButton_);
    actionRow->addWidget(cancelButton_);
    actionRow->addWidget(openOutputButton_);
    actionRow->addStretch(1);
    actionRow->addWidget(clearLogButton_);
    root->addLayout(actionRow);

    progressBar_ = new QProgressBar(central);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    root->addWidget(progressBar_);

    statusLabel_ = new QLabel(tr("Ready."), central);
    root->addWidget(statusLabel_);

    // --- results, preview and log -------------------------------------------
    resultList_ = new QListWidget(central);
    resultList_->setUniformItemSizes(true);
    resultList_->setToolTip(tr("Textures converted by the last run."));

    previewLabel_ = new QLabel(tr("Select a converted texture to preview it"), central);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(260, 260);
    previewLabel_->setStyleSheet(QStringLiteral("background:#141414;color:#777777;border:1px solid #333333;"));

    auto* resultsSplitter = new QSplitter(Qt::Horizontal, central);
    resultsSplitter->addWidget(resultList_);
    resultsSplitter->addWidget(previewLabel_);
    resultsSplitter->setStretchFactor(0, 1);
    resultsSplitter->setStretchFactor(1, 1);

    logView_ = new QPlainTextEdit(central);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(20000);
    logView_->setStyleSheet(
        QStringLiteral("background:#1b1b1b;color:#d8d8d8;font-family:Consolas,'DejaVu Sans Mono',monospace;"));

    auto* verticalSplitter = new QSplitter(Qt::Vertical, central);
    verticalSplitter->addWidget(resultsSplitter);
    verticalSplitter->addWidget(logView_);
    verticalSplitter->setStretchFactor(0, 2);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setSizes({600, 240});

    root->addWidget(verticalSplitter, 1);
    setCentralWidget(central);
    resize(1100, 820);

    // --- connections ---------------------------------------------------------
    connect(cacheBrowseButton_, &QPushButton::clicked, this, &MainWindow::chooseCacheDir);
    connect(outputBrowseButton_, &QPushButton::clicked, this, &MainWindow::chooseOutputDir);
    connect(cacheDirEdit_, &QLineEdit::editingFinished, this, &MainWindow::onCacheDirEdited);
    connect(outputDirEdit_, &QLineEdit::editingFinished, this, &MainWindow::onOutputDirEdited);
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::startBatch);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelBatch);
    connect(openOutputButton_, &QPushButton::clicked, this, &MainWindow::openOutputDir);
    connect(clearLogButton_, &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(resultList_, &QListWidget::itemSelectionChanged, this, &MainWindow::onResultSelected);

}

void MainWindow::loadSettings() {
    QSettings settings(QStringLiteral("SLTextureDecoder"), QStringLiteral("GUI"));
    cacheDirEdit_->setText(settings.value(QStringLiteral("cacheDir")).toString());
    outputDirEdit_->setText(settings.value(QStringLiteral("outputDir")).toString());
    completeOnlyCheck_->setChecked(settings.value(QStringLiteral("completeOnly"), false).toBool());
    overwriteCheck_->setChecked(settings.value(QStringLiteral("overwrite"), false).toBool());
    verboseCheck_->setChecked(settings.value(QStringLiteral("verbose"), false).toBool());
    jobsSpin_->setValue(settings.value(QStringLiteral("jobs"), jobsSpin_->value()).toInt());
    limitSpin_->setValue(settings.value(QStringLiteral("limit"), 0).toInt());
    showCacheInfo();
}

void MainWindow::saveSettings() const {
    QSettings settings(QStringLiteral("SLTextureDecoder"), QStringLiteral("GUI"));
    settings.setValue(QStringLiteral("cacheDir"), cacheDirEdit_->text().trimmed());
    settings.setValue(QStringLiteral("outputDir"), outputDirEdit_->text().trimmed());
    settings.setValue(QStringLiteral("completeOnly"), completeOnlyCheck_->isChecked());
    settings.setValue(QStringLiteral("overwrite"), overwriteCheck_->isChecked());
    settings.setValue(QStringLiteral("verbose"), verboseCheck_->isChecked());
    settings.setValue(QStringLiteral("jobs"), jobsSpin_->value());
    settings.setValue(QStringLiteral("limit"), limitSpin_->value());
}

void MainWindow::setRunning(bool running) {
    startButton_->setEnabled(!running);
    cancelButton_->setEnabled(running);
    openOutputButton_->setEnabled(!running && !lastOutDir_.isEmpty());
    cacheDirEdit_->setEnabled(!running);
    outputDirEdit_->setEnabled(!running);
    cacheBrowseButton_->setEnabled(!running);
    outputBrowseButton_->setEnabled(!running);
    completeOnlyCheck_->setEnabled(!running);
    overwriteCheck_->setEnabled(!running);
    jobsSpin_->setEnabled(!running);
    limitSpin_->setEnabled(!running);
}

void MainWindow::appendLog(const QString& line, const QString& color) {
    logView_->appendHtml(QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, line.toHtmlEscaped()));
}

QString MainWindow::cacheDir() const {
    return cacheDirEdit_->text().trimmed();
}

QString MainWindow::outputDir() const {
    const QString text = outputDirEdit_->text().trimmed();
    if (!text.isEmpty()) {
        return text;
    }
    const QString cache = cacheDir();
    return cache.isEmpty() ? QString() : cache + QStringLiteral("/png");
}

void MainWindow::showCacheInfo() {
    const QString dir = cacheDir();
    if (dir == inspectedDir_) {
        return;
    }
    inspectedDir_ = dir;

    if (dir.isEmpty()) {
        cacheInfoLabel_->setText(tr("Pick the cache folder to see what it holds."));
        return;
    }

    try {
        const cache::TextureCacheReader reader = cache::TextureCacheReader::open(toPath(dir));
        const std::size_t decodable = reader.decodableIndices(false).size();
        cacheInfoLabel_->setText(tr("%1 records, %2 decodable textures, encoder %3")
                                     .arg(static_cast<qulonglong>(reader.entries().size()))
                                     .arg(static_cast<qulonglong>(decodable))
                                     .arg(QString::fromStdString(reader.info().encoderVersion)));
    } catch (const Error& error) {
        cacheInfoLabel_->setText(tr("Not a usable cache: %1").arg(QString::fromStdString(error.toUserMessage())));
    } catch (const std::exception& error) {
        cacheInfoLabel_->setText(tr("Not a usable cache: %1").arg(QString::fromStdString(error.what())));
    }
}

void MainWindow::stopWorker() {
    if (workerThread_ == nullptr) {
        return;
    }

    workerThread_->quit();
    workerThread_->wait();

    // The thread has finished, so nothing can be using the objects any more.
    delete worker_;
    delete workerThread_;
    worker_ = nullptr;
    workerThread_ = nullptr;
}

void MainWindow::chooseCacheDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select the texture cache folder"), cacheDir());
    if (dir.isEmpty()) {
        return;
    }
    cacheDirEdit_->setText(dir);
    if (outputDirEdit_->text().trimmed().isEmpty()) {
        outputDirEdit_->setText(dir + QStringLiteral("/png"));
    }
    showCacheInfo();
    saveSettings();
}

void MainWindow::chooseOutputDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select the output folder"), outputDir());
    if (dir.isEmpty()) {
        return;
    }
    outputDirEdit_->setText(dir);
    saveSettings();
}

void MainWindow::onCacheDirEdited() {
    showCacheInfo();
    saveSettings();
}

void MainWindow::onOutputDirEdited() {
    saveSettings();
}

void MainWindow::openOutputDir() {
    if (!lastOutDir_.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(lastOutDir_));
    }
}

void MainWindow::clearLog() {
    logView_->clear();
}

void MainWindow::startBatch() {
    if (workerThread_ != nullptr && workerThread_->isRunning()) {
        return;
    }

    const QString cache = cacheDir();
    if (cache.isEmpty()) {
        QMessageBox::warning(this, tr("No cache folder"),
                             tr("Choose the folder that holds texture.entries, texture.cache and the shards."));
        return;
    }

    batch::BatchOptions options;
    options.cacheDir = toPath(cache);
    options.outDir = toPath(outputDir());
    options.completeOnly = completeOnlyCheck_->isChecked();
    options.overwrite = overwriteCheck_->isChecked();
    options.jobs = static_cast<unsigned>(jobsSpin_->value());
    options.limit = static_cast<std::uint32_t>(limitSpin_->value());

    // A fresh thread and worker per run: the previous pair is deleted as soon
    // as its run ends, so nothing can point at a finished object.
    workerThread_ = new QThread();
    worker_ = new BatchWorker();
    worker_->setOptions(options);
    worker_->setVerbose(verboseCheck_->isChecked());
    worker_->moveToThread(workerThread_);

    connect(workerThread_, &QThread::started, worker_, &BatchWorker::process);
    connect(worker_, &BatchWorker::logMessage, this, &MainWindow::onLogMessage);
    connect(worker_, &BatchWorker::textureWritten, this, &MainWindow::onTextureWritten);
    connect(worker_, &BatchWorker::progressChanged, this, &MainWindow::onProgress);
    connect(worker_, &BatchWorker::finished, this, &MainWindow::onFinished);
    connect(worker_, &BatchWorker::failedToStart, this, &MainWindow::onFailedToStart);

    resultList_->clear();
    resultItems_ = 0;
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(tr("Converting..."));
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Starting..."));

    appendLog(tr("--- converting %1 -> %2").arg(cache, fromPath(options.outDir)), QString::fromLatin1(kHintColor));

    saveSettings();
    setRunning(true);
    workerThread_->start();
}

void MainWindow::cancelBatch() {
    if (worker_ != nullptr) {
        worker_->cancel();
        statusLabel_->setText(tr("Cancelling..."));
        appendLog(tr("--- cancel requested"), QString::fromLatin1(kWarningColor));
    }
}

void MainWindow::onLogMessage(int level, const QString& message) {
    QString color = QString::fromLatin1(kInfoColor);
    switch (static_cast<LogLevel>(level)) {
    case LogLevel::Verbose:
        color = QString::fromLatin1(kVerboseColor);
        break;
    case LogLevel::Warning:
        color = QString::fromLatin1(kWarningColor);
        break;
    case LogLevel::Error:
        color = QString::fromLatin1(kErrorColor);
        break;
    case LogLevel::Info:
        break;
    }

    const QString tag = QString::fromLatin1(logLevelTag(static_cast<LogLevel>(level)));
    appendLog(QStringLiteral("[%1] %2").arg(tag, message), color);
}

void MainWindow::onTextureWritten(quint32 index, const QString& file, quint32 width, quint32 height,
                                  quint32 components, bool complete, quint64 bytes) {
    if (resultItems_ >= kMaxListedResults) {
        return;
    }

    const QString label = QFileInfo(file).fileName() + QStringLiteral("   ") + QString::number(width) +
                          QStringLiteral("x") + QString::number(height) + QStringLiteral("   ") +
                          QString::number(components) + QStringLiteral(" comp") +
                          (complete ? QString() : QStringLiteral(" (partial)")) + QStringLiteral("   ") +
                          formatBytes(bytes);
    auto* item = new QListWidgetItem(label, resultList_);
    item->setData(Qt::UserRole, file);
    item->setData(Qt::UserRole + 1, index);
    ++resultItems_;
}

void MainWindow::onProgress(quint64 records, quint64 selected, quint64 done, quint64 written, quint64 partial,
                            quint64 skipped, quint64 failed, quint64 bytes) {
    const quint64 total = std::max<quint64>(1, selected);
    progressBar_->setMaximum(static_cast<int>(total));
    progressBar_->setValue(static_cast<int>(std::min<quint64>(done, total)));
    statusLabel_->setText(tr("%1 of %2 converted - %3 written (%4 best effort), %5 skipped, %6 failed, %7 of %8 "
                             "records in the cache")
                              .arg(done)
                              .arg(selected)
                              .arg(written)
                              .arg(partial)
                              .arg(skipped)
                              .arg(failed)
                              .arg(formatBytes(bytes))
                              .arg(records));
}

void MainWindow::onFinished(quint64 records, quint64 selected, quint64 written, quint64 partial, quint64 skipped,
                            quint64 failed, quint64 bytes, bool cancelled, const QString& outDir) {
    Q_UNUSED(records)

    stopWorker();
    lastOutDir_ = outDir;
    setRunning(false);

    const QString headline = tr("%1: %2 of %3 textures written (%4 best effort), %5 skipped, %6 failed, %7")
                                 .arg(cancelled ? tr("Cancelled") : tr("Finished"))
                                 .arg(written)
                                 .arg(selected)
                                 .arg(partial)
                                 .arg(skipped)
                                 .arg(failed)
                                 .arg(formatBytes(bytes));

    statusLabel_->setText(headline);
    appendLog(headline, QString::fromLatin1(failed == 0 ? kHintColor : kWarningColor));
    previewLabel_->setPixmap(QPixmap());
    previewLabel_->setText(written == 0 ? tr("No texture was converted") : tr("%1 PNG written").arg(written));
    if (!lastOutDir_.isEmpty()) {
        statusBar()->showMessage(tr("Output: %1").arg(lastOutDir_), 10000);
    }
    saveSettings();
}

void MainWindow::onFailedToStart(const QString& message) {
    stopWorker();
    setRunning(false);
    statusLabel_->setText(message);
    appendLog(message, QString::fromLatin1(kErrorColor));
    QMessageBox::critical(this, tr("Conversion failed"), message);
}

void MainWindow::onResultSelected() {
    const QListWidgetItem* item = resultList_->currentItem();
    if (item == nullptr) {
        return;
    }

    const QString file = item->data(Qt::UserRole).toString();
    QImageReader reader(file);
    if (!reader.canRead()) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(tr("Cannot read %1").arg(QFileInfo(file).fileName()));
        return;
    }

    const QSize size = reader.size();
    if (size.width() > kPreviewSize || size.height() > kPreviewSize) {
        reader.setScaledSize(size.scaled(kPreviewSize, kPreviewSize, Qt::KeepAspectRatio));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(tr("Cannot read %1").arg(QFileInfo(file).fileName()));
        return;
    }

    previewLabel_->setText(QString());
    previewLabel_->setPixmap(QPixmap::fromImage(image));
    previewLabel_->setToolTip(QFileInfo(file).fileName());
}

} // namespace sltcd::gui
