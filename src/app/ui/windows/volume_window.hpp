// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_owner.hpp"

#include <QMainWindow>

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
    void setVolume(std::shared_ptr<const fluvel_ip::VolumeOwner> volume);

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

    /// Loads the files (with a progress dialog) and displays the volume.
    void loadFiles(const QStringList& files, const QString& sourceName);

    QDockWidget* addSliceDock(const QString& title, const QString& objectName, fluvel_ip::VolumeAxis axis,
                              VolumeSliceView*& view);
    void setupDocks();

    std::shared_ptr<const fluvel_ip::VolumeOwner> volume_;

    VolumeSliceView* axialView_{nullptr};
    VolumeSliceView* coronalView_{nullptr};
    VolumeSliceView* sagittalView_{nullptr};

    QAction* openSeriesAct_{nullptr};
    QAction* openFolderAct_{nullptr};
};

} // namespace fluvel
