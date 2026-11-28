// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_window.hpp"

#include "contrast_control.hpp"
#include "raw_volume_dialog.hpp"
#include "volume_loader.hpp"
#include "volume_slice_view.hpp"

#include "file_utils.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QShowEvent>
#include <QStatusBar>

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
fluvel_ip::VolumeOwner<std::uint8_t> makeTestVolume(int size)
{
    size = std::max(size, 8);

    fluvel_ip::VolumeOwner<std::uint8_t> volume(size, size, size, 20);

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

    setupMenus();
    setupDocks();

    setVolume(std::make_shared<const AnyVolume>(makeTestVolume(96)));

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
    auto* axialDock =
        addSliceDock(tr("Axial"), "volume_dock_axial", fluvel_ip::VolumeAxis::Axial, axialView_);
    auto* coronalDock = addSliceDock(tr("Coronal"), "volume_dock_coronal",
                                     fluvel_ip::VolumeAxis::Coronal, coronalView_);
    auto* sagittalDock = addSliceDock(tr("Sagittal"), "volume_dock_sagittal",
                                      fluvel_ip::VolumeAxis::Sagittal, sagittalView_);

    auto* viewer3dDock = new QDockWidget(tr("3D"), this);
    viewer3dDock->setObjectName("volume_dock_3d");

    auto* placeholder = new QLabel(tr("3D view (work in progress)"), viewer3dDock);
    placeholder->setAlignment(Qt::AlignCenter);
    viewer3dDock->setWidget(placeholder);

    contrastControl_ = new ContrastControl(this);

    auto* contrastDock = new QDockWidget(tr("Contrast"), this);
    contrastDock->setObjectName("volume_dock_contrast");
    contrastDock->setWidget(contrastControl_);

    for (VolumeSliceView* view : {axialView_, coronalView_, sagittalView_})
        connect(view, &VolumeSliceView::windowDragged, contrastControl_,
                &ContrastControl::dragWindow);

    connect(contrastControl_, &ContrastControl::windowChanged, this,
            [this](const fluvel_ip::VoxelWindow& window)
            {
                axialView_->setWindow(window);
                coronalView_->setWindow(window);
                sagittalView_->setWindow(window);
            });

    // Default layout: 2 x 2.
    addDockWidget(Qt::TopDockWidgetArea, axialDock);
    splitDockWidget(axialDock, coronalDock, Qt::Horizontal);

    addDockWidget(Qt::BottomDockWidgetArea, sagittalDock);
    splitDockWidget(sagittalDock, viewer3dDock, Qt::Horizontal);

    // The contrast sits under the 3D view, in the bottom right cell of the 2 x 2 grid.
    splitDockWidget(viewer3dDock, contrastDock, Qt::Vertical);
}

void VolumeWindow::setupMenus()
{
    openSeriesAct_ = new QAction(tr("Open image &series..."), this);
    openSeriesAct_->setShortcut(QKeySequence::Open);
    openSeriesAct_->setStatusTip(
        tr("Load several images (or one multi-page TIFF) as a volume, one image per slice."));

    openFolderAct_ = new QAction(tr("Open &folder..."), this);
    openFolderAct_->setStatusTip(tr("Load all the images of a folder as a volume."));

    openRawAct_ = new QAction(tr("Open &raw volume..."), this);
    openRawAct_->setStatusTip(
        tr("Load a raw file of voxels. Its sizes and voxel type are asked, since the "
           "file does not contain them."));

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openSeriesAct_);
    fileMenu->addAction(openFolderAct_);
    fileMenu->addSeparator();
    fileMenu->addAction(openRawAct_);

    connect(openSeriesAct_, &QAction::triggered, this, &VolumeWindow::openImageSeries);
    connect(openFolderAct_, &QAction::triggered, this, &VolumeWindow::openFolder);
    connect(openRawAct_, &QAction::triggered, this, &VolumeWindow::openRawVolume);
}

void VolumeWindow::openImageSeries()
{
    QSettings settings;
    const QString start =
        settings.value("history/last_volume_directory", QDir::homePath()).toString();

    const QString filter = tr("Images (%1)").arg(file_utils::supportedImageExtensions());

    const QStringList files =
        QFileDialog::getOpenFileNames(this, tr("Open image series"), start, filter);

    if (files.isEmpty())
        return;

    settings.setValue("history/last_volume_directory", QFileInfo(files.first()).absolutePath());

    // A single file is a multi-page file: its pages are the slices.
    const QStringList ordered = files.size() == 1 ? files : volume_loader::sortedNaturally(files);

    loadFiles(ordered, QFileInfo(ordered.first()).fileName());
}

void VolumeWindow::openFolder()
{
    QSettings settings;
    const QString start =
        settings.value("history/last_volume_directory", QDir::homePath()).toString();

    const QString folder = QFileDialog::getExistingDirectory(this, tr("Open folder"), start);

    if (folder.isEmpty())
        return;

    settings.setValue("history/last_volume_directory", folder);

    const QStringList files = volume_loader::imageFilesInFolder(folder);

    if (files.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("No supported image found in this folder."));
        return;
    }

    loadFiles(files, QFileInfo(folder).fileName());
}

void VolumeWindow::loadFiles(const QStringList& files, const QString& sourceName)
{
    runLoad(
        [&files](const volume_loader::ProgressCallback& progress)
        {
            return volume_loader::loadImageSeries(files, progress);
        },
        sourceName);
}

void VolumeWindow::openRawVolume()
{
    QSettings settings;
    const QString start =
        settings.value("history/last_volume_directory", QDir::homePath()).toString();

    const QString filter = tr("Raw volumes (*.raw *.bin *.vol *.img *.dat);;All files (*)");

    const QString path = QFileDialog::getOpenFileName(this, tr("Open raw volume"), start, filter);

    if (path.isEmpty())
        return;

    settings.setValue("history/last_volume_directory", QFileInfo(path).absolutePath());

    if (const QString problem = file_utils::readableFileError(path); !problem.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), problem);
        return;
    }

    const auto parameters = RawVolumeDialog::ask(path, this);

    if (!parameters.has_value())
        return;

    runLoad(
        [&path, &parameters](const volume_loader::ProgressCallback& progress)
        {
            return volume_loader::loadRaw(path, *parameters, progress);
        },
        QFileInfo(path).fileName());
}

void VolumeWindow::runLoad(
    const std::function<volume_loader::Result(const volume_loader::ProgressCallback&)>& job,
    const QString& sourceName)
{
    QProgressDialog progress(tr("Loading the volume..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(400);

    const auto result = job(
        [&progress](int done, int total)
        {
            progress.setMaximum(total);
            progress.setValue(done);

            return !progress.wasCanceled();
        });

    progress.close();

    if (result.cancelled)
        return;

    if (!result.ok())
    {
        QMessageBox::warning(this, tr("Error"), result.error);
        return;
    }

    setVolume(result.volume);

    setWindowTitle(tr("Fluvel Set - Volume - %1").arg(sourceName));
    statusBar()->showMessage(tr("Volume loaded: %1 x %2 x %3")
                                 .arg(result.volume->width())
                                 .arg(result.volume->height())
                                 .arg(result.volume->depth()),
                             8000);
}

void VolumeWindow::setVolume(std::shared_ptr<const AnyVolume> volume)
{
    volume_ = std::move(volume);

    axialView_->setVolume(volume_);
    coronalView_->setVolume(volume_);
    sagittalView_->setVolume(volume_);

    // The views start with the default window of the volume: the control must show the same.
    if (volume_ && !volume_->isEmpty())
    {
        const auto [lowest, highest] = volume_->valueRange();
        contrastControl_->setLimits(lowest, highest, volume_->defaultWindow(),
                                    volume_->type() != VoxelType::Float32);
    }
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
