// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "preferences_dialog.hpp"

#include "application_settings.hpp"
#include "file_utils.hpp"
#include "recording_buffer_settings.hpp"
#include "video_exporter.hpp"
#include "video_exporter_utils.hpp"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

namespace fluvel
{

namespace
{

// A path typed by hand gets no access inside the Flatpak sandbox: only a folder
// picked with the file dialog (through the portal) is granted. The directory
// fields are therefore read-only there, and "Browse..." stays the only way in.
void lockDirectoryEditInFlatpak(QLineEdit* edit)
{
    if (qEnvironmentVariableIsSet("FLATPAK_ID"))
        edit->setReadOnly(true);
}

QString previewTimestamp()
{
    return "_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
}

QString previewBaseName(const QString& baseName, bool appendTimestamp)
{
    QString name = baseName;

    if (appendTimestamp)
        name += previewTimestamp();

    return name;
}

QString filenamePreview(const QString& baseName, const QString& extension, bool appendTimestamp)
{
    return previewBaseName(baseName, appendTimestamp) + "." + extension;
}

QString directoryPreview(const QString& baseName, bool appendTimestamp)
{
    return previewBaseName(baseName, appendTimestamp) + QDir::separator();
}

} // namespace

void PreferencesDialog::showDialog()
{
    static PreferencesDialog dialog;

    // Show the preferences actually applied, not the leftovers of an earlier
    // cancelled edit. A dialog that is already visible keeps its pending edits.
    if (!dialog.isVisible())
        dialog.loadFromSettings();

    dialog.show();
    dialog.raise();
    dialog.activateWindow();
}

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));

    QSettings settings;

    const auto geometry = settings.value("ui_geometry/preferences_dialog").toByteArray();

    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    auto* dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    dialogButtons->setCenterButtons(true);

    connect(dialogButtons, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);

    connect(dialogButtons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            &PreferencesDialog::restoreDefaults);

    auto* layout = new QVBoxLayout(this);

    layout->addWidget(createLanguageSection());
    layout->addWidget(createSnapshotSection());
    layout->addWidget(createVideoRecordingSection());
    layout->addWidget(createRecordingBufferSection());

    layout->addWidget(dialogButtons);

    layout->setSizeConstraint(QLayout::SetMinimumSize);

    loadFromSettings();
}

QWidget* PreferencesDialog::createLanguageSection()
{
    auto* group = new QGroupBox(tr("Language"), this);
    auto* layout = new QVBoxLayout(group);

    languageCombo_ = new QComboBox(group);

    const QString locale = QLocale::system().name().section('_', 0, 0);

    languageCombo_->addItem(tr("System (%1)").arg(locale),
                            QVariant::fromValue(int(Language::System)));

    languageCombo_->addItem(tr("English"), QVariant::fromValue(int(Language::English)));
    languageCombo_->addItem(tr("French"), QVariant::fromValue(int(Language::French)));

    // Native names, deliberately not translated: a user must recognize their own language.
    languageCombo_->addItem(QStringLiteral("Español"), QVariant::fromValue(int(Language::Spanish)));
    languageCombo_->addItem(QStringLiteral("Deutsch"), QVariant::fromValue(int(Language::German)));
    languageCombo_->addItem(QStringLiteral("简体中文"),
                            QVariant::fromValue(int(Language::ChineseSimplified)));
    languageCombo_->addItem(QStringLiteral("Português (Brasil)"),
                            QVariant::fromValue(int(Language::PortugueseBrazil)));
    languageCombo_->addItem(QStringLiteral("Português (Portugal)"),
                            QVariant::fromValue(int(Language::PortuguesePortugal)));
    languageCombo_->addItem(QStringLiteral("Italiano"), QVariant::fromValue(int(Language::Italian)));
    languageCombo_->addItem(QStringLiteral("Русский"), QVariant::fromValue(int(Language::Russian)));
    languageCombo_->addItem(QStringLiteral("日本語"), QVariant::fromValue(int(Language::Japanese)));
    languageCombo_->addItem(QStringLiteral("한국어"), QVariant::fromValue(int(Language::Korean)));
    languageCombo_->addItem(QStringLiteral("Polski"), QVariant::fromValue(int(Language::Polish)));
    languageCombo_->addItem(QStringLiteral("Nederlands"), QVariant::fromValue(int(Language::Dutch)));
    languageCombo_->addItem(QStringLiteral("Türkçe"), QVariant::fromValue(int(Language::Turkish)));
    languageCombo_->addItem(QStringLiteral("Bahasa Indonesia"),
                            QVariant::fromValue(int(Language::Indonesian)));
    languageCombo_->addItem(QStringLiteral("Tiếng Việt"), QVariant::fromValue(int(Language::Vietnamese)));

    auto* restartLabel = new QLabel(
        tr("The language change will take effect after restarting the application."), group);

    restartLabel->setWordWrap(true);

    layout->addWidget(languageCombo_);
    layout->addWidget(restartLabel);

    return group;
}

QWidget* PreferencesDialog::createSnapshotSection()
{
    auto* group = new QGroupBox(tr("Snapshots"), this);
    auto* layout = new QFormLayout(group);

    snapshotDirectoryEdit_ = new QLineEdit(group);
    lockDirectoryEditInFlatpak(snapshotDirectoryEdit_);
    snapshotBaseNameEdit_ = new QLineEdit(group);
    snapshotFormatCombo_ = new QComboBox(group);
    snapshotTimestampCheck_ = new QCheckBox(tr("Append timestamp"), group);
    snapshotPreviewLabel_ = new QLabel(group);

    auto* browseButton = new QPushButton(tr("Browse..."), group);

    auto* directoryLayout = new QHBoxLayout;
    directoryLayout->addWidget(snapshotDirectoryEdit_, 1);
    directoryLayout->addWidget(browseButton);

    const QList<QByteArray> formats = file_utils::writableImageFormats();

    for (const QByteArray& format : formats)
    {
        snapshotFormatCombo_->addItem(QString::fromLatin1(format).toUpper(), format);
    }

    layout->addRow(tr("Directory:"), directoryLayout);
    layout->addRow(tr("Base name:"), snapshotBaseNameEdit_);
    layout->addRow(tr("Format:"), snapshotFormatCombo_);
    layout->addRow(QString(), snapshotTimestampCheck_);
    layout->addRow(tr("Preview:"), snapshotPreviewLabel_);

    connect(browseButton, &QPushButton::clicked, this, &PreferencesDialog::selectSnapshotDirectory);

    connect(snapshotBaseNameEdit_, &QLineEdit::textChanged, this,
            &PreferencesDialog::updateSnapshotPreview);

    connect(snapshotFormatCombo_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateSnapshotPreview);

    connect(snapshotTimestampCheck_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateSnapshotPreview);

    return group;
}

QWidget* PreferencesDialog::createVideoRecordingSection()
{
    auto* group = new QGroupBox(tr("Video recording"), this);
    auto* layout = new QFormLayout(group);

    videoDirectoryEdit_ = new QLineEdit(group);
    lockDirectoryEditInFlatpak(videoDirectoryEdit_);
    videoBaseNameEdit_ = new QLineEdit(group);
    videoCodecCombo_ = new QComboBox(group);
    videoTimestampCheck_ = new QCheckBox(tr("Append timestamp"), group);
    videoPreviewLabel_ = new QLabel(group);

    auto* browseButton = new QPushButton(tr("Browse..."), group);

    auto* directoryLayout = new QHBoxLayout;
    directoryLayout->addWidget(videoDirectoryEdit_, 1);
    directoryLayout->addWidget(browseButton);

    VideoExporter exporter;

    const QList<VideoCodec> codecs = exporter.availableCodecs();

    for (VideoCodec codec : codecs)
    {
        videoCodecCombo_->addItem(exporter_utils::toString(codec), int(codec));
    }

    convertMp4Check_ = new QCheckBox(tr("Convert to standard MP4 when recording ends"), group);
    convertMp4Check_->setToolTip(
        tr("MP4 files are recorded in a fragmented form so they stay playable after a crash. "
           "This option rewrites them as standard MP4 files when the recording ends, which "
           "improves compatibility with some players and editors. The conversion does not "
           "re-encode the video, but it rewrites the whole file: it takes extra time and "
           "extra disk writes, especially for long recordings."));

    recordingModeCombo_ = new QComboBox(group);
    recordingModeCombo_->addItem(tr("Single file"), int(RecordingMode::SingleFile));
    recordingModeCombo_->addItem(tr("Circular"), int(RecordingMode::Circular));

    retentionTimeLabel_ = new QLabel(tr("Retention time:"), group);
    retentionTimeSpin_ = new QSpinBox(group);
    retentionTimeSpin_->setRange(1, 24 * 60);
    retentionTimeSpin_->setSuffix(tr(" min"));

    segmentCountLabel_ = new QLabel(tr("Segments:"), group);
    segmentCountSpin_ = new QSpinBox(group);
    segmentCountSpin_->setRange(2, 1000);

    segmentDurationTextLabel_ = new QLabel(tr("Segment duration:"), group);
    segmentDurationLabel_ = new QLabel(group);

    layout->addRow(tr("Directory:"), directoryLayout);
    layout->addRow(tr("Base name:"), videoBaseNameEdit_);
    layout->addRow(tr("Codec:"), videoCodecCombo_);
    layout->addRow(QString(), videoTimestampCheck_);
    layout->addRow(tr("Preview:"), videoPreviewLabel_);
    layout->addRow(tr("Recording mode:"), recordingModeCombo_);
    layout->addRow(QString(), convertMp4Check_);
    layout->addRow(retentionTimeLabel_, retentionTimeSpin_);
    layout->addRow(segmentCountLabel_, segmentCountSpin_);
    layout->addRow(segmentDurationTextLabel_, segmentDurationLabel_);

    connect(videoCodecCombo_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateConvertOption);

    connect(browseButton, &QPushButton::clicked, this, &PreferencesDialog::selectVideoDirectory);

    connect(videoBaseNameEdit_, &QLineEdit::textChanged, this,
            &PreferencesDialog::updateVideoPreview);

    connect(videoCodecCombo_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateVideoPreview);

    connect(videoTimestampCheck_, &QCheckBox::toggled, this,
            &PreferencesDialog::updateVideoPreview);

    connect(retentionTimeSpin_, &QSpinBox::valueChanged, this,
            &PreferencesDialog::updateSegmentDuration);

    connect(segmentCountSpin_, &QSpinBox::valueChanged, this,
            &PreferencesDialog::updateSegmentDuration);

    connect(recordingModeCombo_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateVideoPreview);

    connect(recordingModeCombo_, &QComboBox::currentIndexChanged, this,
            &PreferencesDialog::updateCircularControls);

    return group;
}

QWidget* PreferencesDialog::createRecordingBufferSection()
{
    auto* groupBox = new QGroupBox(tr("Recording buffer"));

    auto* layout = new QFormLayout(groupBox);

    recordingRamSpin_ = new QSpinBox(groupBox);
    recordingRamSpin_->setRange(128, 32768);
    recordingRamSpin_->setSuffix(tr(" MiB"));

    recordingDiskSpin_ = new QSpinBox(groupBox);
    recordingDiskSpin_->setRange(0, 1024);
    recordingDiskSpin_->setSuffix(tr(" GiB"));

    recordingOverflowCombo_ = new QComboBox(groupBox);
    recordingOverflowCombo_->addItem(tr("Stop recording"),
                                     int(BufferOverflowPolicy::StopRecording));

    recordingOverflowCombo_->addItem(tr("Discard oldest frames"),
                                     int(BufferOverflowPolicy::DiscardOldest));

    layout->addRow(tr("Maximum RAM usage:"), recordingRamSpin_);
    layout->addRow(tr("Maximum temporary storage usage:"), recordingDiskSpin_);
    layout->addRow(tr("When buffer is full:"), recordingOverflowCombo_);

    return groupBox;
}

void PreferencesDialog::loadFromSettings()
{
    const auto& settings = ApplicationSettings::instance();

    showValues(settings.appLanguage(), settings.snapshotPreferences(),
               settings.videoRecordingPreferences(), settings.recordingBufferSettings());
}

void PreferencesDialog::restoreDefaults()
{
    // The language goes back to the system one; as for any language change,
    // it only takes effect after restarting the application.
    showValues(Language::System, ApplicationSettings::defaultSnapshotPreferences(),
               ApplicationSettings::defaultVideoRecordingPreferences(), RecordingBufferSettings{});
}

void PreferencesDialog::showValues(Language language, const SnapshotPreferences& snapshot,
                                   const VideoRecordingPreferences& video,
                                   const RecordingBufferSettings& buffer)
{
    // Language

    const int languageIndex = languageCombo_->findData(QVariant::fromValue(int(language)));

    if (languageIndex >= 0)
        languageCombo_->setCurrentIndex(languageIndex);

    // Snapshots

    snapshotDirectoryEdit_->setText(snapshot.directory);
    snapshotBaseNameEdit_->setText(snapshot.baseName);

    const int formatIndex = snapshotFormatCombo_->findData(snapshot.preferredFormat);

    if (formatIndex >= 0)
        snapshotFormatCombo_->setCurrentIndex(formatIndex);

    snapshotTimestampCheck_->setChecked(snapshot.appendTimestamp);

    // Video recording

    videoDirectoryEdit_->setText(video.directory);
    videoBaseNameEdit_->setText(video.baseName);

    const int codecIndex = videoCodecCombo_->findData(int(video.preferredCodec));

    if (codecIndex >= 0)
        videoCodecCombo_->setCurrentIndex(codecIndex);

    videoTimestampCheck_->setChecked(video.appendTimestamp);
    convertMp4Check_->setChecked(video.convertToStandardMp4);

    recordingModeCombo_->setCurrentIndex(recordingModeCombo_->findData(int(video.recordingMode)));

    retentionTimeSpin_->setValue(video.retentionTimeMinutes);
    segmentCountSpin_->setValue(video.segmentCount);

    // Recording buffer

    recordingRamSpin_->setValue(int(buffer.maxRamUsage / (1024ULL * 1024ULL)));
    recordingDiskSpin_->setValue(int(buffer.maxDiskUsage / (1024ULL * 1024ULL * 1024ULL)));

    recordingOverflowCombo_->setCurrentIndex(
        recordingOverflowCombo_->findData(int(buffer.overflowPolicy)));

    // Setting a value emits a change signal only when it differs from the
    // current one, so refresh the derived state explicitly.
    updateSnapshotPreview();
    updateVideoPreview();
    updateSegmentDuration();
    updateConvertOption();
    updateCircularControls();
}

void PreferencesDialog::updateConvertOption()
{
    if (videoCodecCombo_->currentIndex() < 0)
        return;

    // Only MP4 recordings can be converted (FFV1 uses MKV, VP9 uses WebM).
    const auto codec = static_cast<VideoCodec>(videoCodecCombo_->currentData().toInt());

    convertMp4Check_->setVisible(exporter_utils::preferredContainer(codec) == VideoContainer::Mp4);
}

void PreferencesDialog::updateCircularControls()
{
    const bool circular =
        recordingModeCombo_->currentData().toInt() == int(RecordingMode::Circular);

    retentionTimeLabel_->setEnabled(circular);
    retentionTimeSpin_->setEnabled(circular);

    segmentCountLabel_->setEnabled(circular);
    segmentCountSpin_->setEnabled(circular);

    segmentDurationTextLabel_->setEnabled(circular);
    segmentDurationLabel_->setEnabled(circular);
}

void PreferencesDialog::selectSnapshotDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Select snapshot directory"), snapshotDirectoryEdit_->text());

    if (!directory.isEmpty())
        snapshotDirectoryEdit_->setText(directory);
}

void PreferencesDialog::selectVideoDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Select video recording directory"), videoDirectoryEdit_->text());

    if (!directory.isEmpty())
        videoDirectoryEdit_->setText(directory);
}

void PreferencesDialog::updateSnapshotPreview()
{
    const QString extension =
        QString::fromLatin1(snapshotFormatCombo_->currentData().toByteArray());

    snapshotPreviewLabel_->setText(filenamePreview(snapshotBaseNameEdit_->text(), extension,
                                                   snapshotTimestampCheck_->isChecked()));
}

void PreferencesDialog::updateVideoPreview()
{
    if (videoCodecCombo_->currentIndex() < 0)
    {
        videoPreviewLabel_->clear();
        return;
    }

    const bool circular =
        recordingModeCombo_->currentData().toInt() == int(RecordingMode::Circular);

    if (circular)
    {
        videoPreviewLabel_->setText(
            directoryPreview(videoBaseNameEdit_->text(), videoTimestampCheck_->isChecked()));
    }
    else
    {
        const auto codec = static_cast<VideoCodec>(videoCodecCombo_->currentData().toInt());

        const VideoContainer container = exporter_utils::preferredContainer(codec);

        videoPreviewLabel_->setText(filenamePreview(videoBaseNameEdit_->text(),
                                                    exporter_utils::expectedExtension(container),
                                                    videoTimestampCheck_->isChecked()));
    }
}

void PreferencesDialog::accept()
{
    auto& settings = ApplicationSettings::instance();

    settings.setAppLanguage(static_cast<Language>(languageCombo_->currentData().toInt()));

    SnapshotPreferences snapshotPreferences;
    snapshotPreferences.directory = snapshotDirectoryEdit_->text();
    snapshotPreferences.baseName = snapshotBaseNameEdit_->text();
    snapshotPreferences.preferredFormat = snapshotFormatCombo_->currentData().toByteArray();
    snapshotPreferences.appendTimestamp = snapshotTimestampCheck_->isChecked();

    settings.setSnapshotPreferences(snapshotPreferences);

    VideoRecordingPreferences videoPreferences;
    videoPreferences.directory = videoDirectoryEdit_->text();
    videoPreferences.baseName = videoBaseNameEdit_->text();

    if (videoCodecCombo_->currentIndex() >= 0)
    {
        videoPreferences.preferredCodec =
            static_cast<VideoCodec>(videoCodecCombo_->currentData().toInt());
    }

    videoPreferences.appendTimestamp = videoTimestampCheck_->isChecked();

    videoPreferences.recordingMode =
        static_cast<RecordingMode>(recordingModeCombo_->currentData().toInt());

    videoPreferences.convertToStandardMp4 = convertMp4Check_->isChecked();

    videoPreferences.retentionTimeMinutes = retentionTimeSpin_->value();

    videoPreferences.segmentCount = segmentCountSpin_->value();

    settings.setVideoRecordingPreferences(videoPreferences);

    auto bufferSettings = settings.recordingBufferSettings();

    bufferSettings.maxRamUsage = std::uint64_t(recordingRamSpin_->value()) * 1024 * 1024;

    bufferSettings.maxDiskUsage = std::uint64_t(recordingDiskSpin_->value()) * 1024 * 1024 * 1024;

    bufferSettings.overflowPolicy =
        static_cast<BufferOverflowPolicy>(recordingOverflowCombo_->currentData().toInt());

    settings.setRecordingBufferSettings(bufferSettings);

    settings.save();

    QDialog::accept();
}

void PreferencesDialog::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("ui_geometry/preferences_dialog", saveGeometry());

    QDialog::closeEvent(event);
}

void PreferencesDialog::updateSegmentDuration()
{
    const int durationSeconds = retentionTimeSpin_->value() * 60 / segmentCountSpin_->value();

    const int minutes = durationSeconds / 60;
    const int seconds = durationSeconds % 60;

    QString text;

    if (minutes > 0)
    {
        text = tr("%1 min").arg(minutes);

        if (seconds > 0)
            text += tr(" %1 s").arg(seconds);
    }
    else
    {
        text = tr("%1 s").arg(seconds);
    }

    segmentDurationLabel_->setText(text);
}

} // namespace fluvel