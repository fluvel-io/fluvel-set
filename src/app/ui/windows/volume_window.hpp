// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "any_volume.hpp"
#include "volume_loader.hpp"

#include <QMainWindow>

#include <functional>
#include <memory>

class QCloseEvent;
class QAction;
class QDockWidget;
class QShowEvent;

namespace fluvel
{

class VolumeSliceView;

/**
 * @brief Window of the volume session.
 *
 * Shows a volume through three orthogonal slice views (axial, coronal, sagittal), each with
 * its own slider, and a fourth dock reserved for the future 3D view. The views are dock
 * widgets: they can be moved, floated or closed, and the layout is saved with the window.
 *
 * Until volumes can be loaded, the window displays a synthetic test volume.
 */
class VolumeWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit VolumeWindow(QWidget* parent = nullptr);

    /// Replaces the displayed volume.
    void setVolume(std::shared_ptr<const AnyVolume> volume);

signals:
    /// Emitted each time the window is shown.
    void volumeWindowShown();

    /// Emitted when the window is closed by the user.
    void volumeWindowClosed();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupMenus();
    void openImageSeries();
    void openFolder();
    void openRawVolume();

    /// Loads the files (with a progress dialog) and displays the volume.
    void loadFiles(const QStringList& files, const QString& sourceName);

    /// Runs a loading job with a progress dialog, then displays the volume or the error.
    void
    runLoad(const std::function<volume_loader::Result(const volume_loader::ProgressCallback&)>& job,
            const QString& sourceName);

    QDockWidget* addSliceDock(const QString& title, const QString& objectName,
                              fluvel_ip::VolumeAxis axis, VolumeSliceView*& view);
    void setupDocks();

    std::shared_ptr<const AnyVolume> volume_;

    VolumeSliceView* axialView_{nullptr};
    VolumeSliceView* coronalView_{nullptr};
    VolumeSliceView* sagittalView_{nullptr};

    QAction* openSeriesAct_{nullptr};
    QAction* openFolderAct_{nullptr};
    QAction* openRawAct_{nullptr};
};

} // namespace fluvel
