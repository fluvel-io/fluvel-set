// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_owner.hpp"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace fluvel::volume_loader
{

/**
 * @brief Outcome of a volume loading.
 *
 * On success @c volume is set. Otherwise @c error describes the failure, except when the
 * user cancelled the loading (@c cancelled is true and @c error is empty).
 */
struct Result
{
    std::shared_ptr<fluvel_ip::VolumeOwner> volume;
    QString error;
    bool cancelled{false};

    bool ok() const
    {
        return volume != nullptr;
    }
};

/**
 * @brief Called after each loaded slice with (loaded, total).
 *
 * Returning false cancels the loading.
 */
using ProgressCallback = std::function<bool(int, int)>;

/**
 * @brief Maximum number of voxels accepted (1 byte per voxel).
 */
constexpr qint64 kMaxVoxels = 2LL * 1024 * 1024 * 1024;

/**
 * @brief Lists the image files of a folder, in natural order (img2 before img10).
 *
 * Only the formats supported by the Qt image plugins are kept. Sub-folders are ignored.
 */
QStringList imageFilesInFolder(const QString& folder);

/**
 * @brief Sorts file paths in natural order (numbers compared by value, not by digits).
 */
QStringList sortedNaturally(QStringList files);

/**
 * @brief Loads a stack of images as an 8-bit grayscale volume.
 *
 * Each file gives one axial slice, in the order given. A single multi-page file (for example
 * a multi-page TIFF) gives one slice per page. Color images are converted to grayscale.
 * All the slices must have the same size.
 *
 * @param files Image files (not sorted by this function, see sortedNaturally()).
 * @param progress Optional progress and cancellation callback.
 */
Result loadImageSeries(const QStringList& files, const ProgressCallback& progress = {});

} // namespace fluvel::volume_loader
