// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "application_settings_types.hpp"
#include "recording_buffer_settings.hpp"

#include <QDialog>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QWidget;
class QSpinBox;

namespace fluvel
{

/**
 * @brief Dialog for configuring persistent application preferences.
 *
 * This dialog allows the user to configure application-wide preferences that
 * are preserved between sessions, including:
 * - the application language,
 * - snapshot output settings,
 * - video recording settings,
 * - recording buffer behavior.
 *
 * Unlike session settings, these preferences are shared by all windows and
 * apply globally to the application.
 *
 * @note The widgets are reloaded from the application settings each time the
 *       dialog is opened, so Cancel (or closing the window) discards the edits
 *       and the next opening shows the applied state. OK writes the values to
 *       the application settings and saves them.
 *
 *       Unlike the session dialogs, this dialog reads and writes the
 *       singleton itself, because it is a single global dialog with no owning
 *       window to relay its values. Reloading at each opening is what keeps its
 *       copy in step, without the limit of the session dialogs.
 */
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Shows the unique preferences dialog.
     *
     * Displays the application's unique preferences dialog. If it is already
     * visible, the dialog is raised and activated instead of creating a new
     * instance.
     *
     */
    static void showDialog();

protected:
    /**
     * @brief Applies the selected user preferences.
     */
    void accept() override;

    /**
     * @brief Handles dialog close events.
     *
     * @param event Close event.
     */
    void closeEvent(QCloseEvent* event) override;

private:
    /**
     * @brief Constructs the user preferences dialog.
     *
     * @param parent Optional parent widget.
     */
    explicit PreferencesDialog(QWidget* parent = nullptr);

    /// Creates the language preferences section.
    QWidget* createLanguageSection();

    /// Creates the snapshot preferences section.
    QWidget* createSnapshotSection();

    /// Creates the video recording preferences section.
    QWidget* createVideoRecordingSection();

    /// Creates the recording buffer preferences section.
    QWidget* createRecordingBufferSection();

    /**
     * @brief Fills every widget from the current application settings.
     *
     * Called when the dialog is built and each time it is opened, so that it
     * always shows the preferences actually applied, even after an earlier
     * edit was cancelled.
     */
    void loadFromSettings();

    /**
     * @brief Fills every widget from the given values.
     *
     * Nothing is written to the application settings: the values only reach
     * them when the user accepts the dialog.
     */
    void showValues(Language language, const SnapshotPreferences& snapshot,
                    const VideoRecordingPreferences& video, const RecordingBufferSettings& buffer);

    /// Fills every widget with the factory values (applied only on OK).
    void restoreDefaults();

    /// Shows the MP4 conversion option only for codecs that record to MP4.
    void updateConvertOption();

    /// Enables the circular-recording controls only in circular mode.
    void updateCircularControls();

    /// Opens a directory selection dialog for snapshots.
    void selectSnapshotDirectory();

    /// Opens a directory selection dialog for video recordings.
    void selectVideoDirectory();

    /// Updates the snapshot filename preview.
    void updateSnapshotPreview();

    /// Updates the video filename preview.
    void updateVideoPreview();

    /**
     * @brief Updates the recording segment duration preview.
     *
     * Computes the duration of a recording segment from the current retention
     * settings.
     */
    void updateSegmentDuration();

    QComboBox* languageCombo_{nullptr};

    QLineEdit* snapshotDirectoryEdit_{nullptr};
    QLineEdit* snapshotBaseNameEdit_{nullptr};
    QComboBox* snapshotFormatCombo_{nullptr};
    QCheckBox* snapshotTimestampCheck_{nullptr};
    QLabel* snapshotPreviewLabel_{nullptr};

    QLineEdit* videoDirectoryEdit_{nullptr};
    QLineEdit* videoBaseNameEdit_{nullptr};
    QComboBox* videoCodecCombo_{nullptr};
    QCheckBox* videoTimestampCheck_{nullptr};
    QLabel* videoPreviewLabel_{nullptr};

    QSpinBox* recordingRamSpin_{nullptr};
    QSpinBox* recordingDiskSpin_{nullptr};
    QComboBox* recordingOverflowCombo_{nullptr};

    QComboBox* recordingModeCombo_{nullptr};

    QCheckBox* convertMp4Check_{nullptr};

    QLabel* retentionTimeLabel_{nullptr};
    QSpinBox* retentionTimeSpin_{nullptr};

    QLabel* segmentCountLabel_{nullptr};
    QSpinBox* segmentCountSpin_{nullptr};

    QLabel* segmentDurationTextLabel_{nullptr};
    QLabel* segmentDurationLabel_{nullptr};
};

} // namespace fluvel