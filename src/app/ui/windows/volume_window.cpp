// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_window.hpp"

#include "volume_slice_view.hpp"

#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QSettings>
#include <QShowEvent>

#include <algorithm>
#include <utility>

namespace fluvel
{

namespace
{

/**
 * @brief Builds a synthetic volume (a sphere with a cavity and a box).
 *
 * Only meant for development, until real volumes can be loaded.
 */
fluvel_ip::VolumeOwner makeTestVolume(int size)
{
    size = std::max(size, 8);

    fluvel_ip::VolumeOwner volume(size, size, size, 20);

    const double c = (size - 1) / 2.0;
    const double outer = size * 0.36;
    const double inner = size * 0.20;

    for (int z = 0; z < size; ++z)
    {
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                const double dx = x - c;
                const double dy = y - c;
                const double dz = z - c;
                const double r2 = dx * dx + dy * dy + dz * dz;

                std::uint8_t value = 20;

                if (r2 <= inner * inner)
                    value = 90; // cavity
                else if (r2 <= outer * outer)
                    value = 220; // shell

                // Off-center box, to tell the three orientations apart.
                if (x > size * 0.70 && y < size * 0.35 && z > size * 0.15 && z < size * 0.45)
                    value = 160;

                volume.at(x, y, z) = value;
            }
        }
    }

    return volume;
}

} // namespace

VolumeWindow::VolumeWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Fluvel Set - Volume"));
    resize(1000, 750);

    // The docks fill the whole window: the central widget is kept empty and collapsed.
    auto* central = new QWidget(this);
    central->setMaximumSize(0, 0);
    setCentralWidget(central);
    setDockNestingEnabled(true);

    setupDocks();

    setVolume(std::make_shared<const fluvel_ip::VolumeOwner>(makeTestVolume(96)));

    QSettings settings;

    if (settings.contains("ui_geometry/volume_window"))
        restoreGeometry(settings.value("ui_geometry/volume_window").toByteArray());

    if (settings.contains("ui_geometry/volume_window_state"))
        restoreState(settings.value("ui_geometry/volume_window_state").toByteArray());
}

QDockWidget* VolumeWindow::addSliceDock(const QString& title, const QString& objectName,
                                        fluvel_ip::VolumeAxis axis, VolumeSliceView*& view)
{
    view = new VolumeSliceView(axis, this);

    auto* dock = new QDockWidget(title, this);
    dock->setObjectName(objectName); // required by saveState()/restoreState()
    dock->setWidget(view);

    return dock;
}

void VolumeWindow::setupDocks()
{
    auto* axialDock = addSliceDock(tr("Axial"), "volume_dock_axial", fluvel_ip::VolumeAxis::Axial, axialView_);
    auto* coronalDock =
        addSliceDock(tr("Coronal"), "volume_dock_coronal", fluvel_ip::VolumeAxis::Coronal, coronalView_);
    auto* sagittalDock =
        addSliceDock(tr("Sagittal"), "volume_dock_sagittal", fluvel_ip::VolumeAxis::Sagittal, sagittalView_);

    auto* viewer3dDock = new QDockWidget(tr("3D"), this);
    viewer3dDock->setObjectName("volume_dock_3d");

    auto* placeholder = new QLabel(tr("3D view (work in progress)"), viewer3dDock);
    placeholder->setAlignment(Qt::AlignCenter);
    viewer3dDock->setWidget(placeholder);

    // Default layout: 2 x 2.
    addDockWidget(Qt::TopDockWidgetArea, axialDock);
    splitDockWidget(axialDock, coronalDock, Qt::Horizontal);

    addDockWidget(Qt::BottomDockWidgetArea, sagittalDock);
    splitDockWidget(sagittalDock, viewer3dDock, Qt::Horizontal);
}

void VolumeWindow::setVolume(std::shared_ptr<const fluvel_ip::VolumeOwner> volume)
{
    volume_ = std::move(volume);

    axialView_->setVolume(volume_);
    coronalView_->setVolume(volume_);
    sagittalView_->setVolume(volume_);
}

void VolumeWindow::showEvent(QShowEvent* event)
{
    emit volumeWindowShown();
    QMainWindow::showEvent(event);
}

void VolumeWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("ui_geometry/volume_window", saveGeometry());
    settings.setValue("ui_geometry/volume_window_state", saveState());

    emit volumeWindowClosed();
    QMainWindow::closeEvent(event);
}

} // namespace fluvel
