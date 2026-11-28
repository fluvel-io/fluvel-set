// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "any_volume.hpp"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

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
    std::shared_ptr<const AnyVolume> volume;
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
 * @brief Maximum size in memory of a loaded volume, in bytes.
 */
constexpr qint64 kMaxVolumeBytes = 2LL * 1024 * 1024 * 1024;

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
 * @brief Layout of a raw volume file: the voxels one after the other, without any header.
 *
 * A raw file does not describe itself, so the sizes, the voxel type and the byte order must be
 * given by the user. The voxels are stored row by row, then slice by slice.
 */
struct RawParameters
{
    int width{0};
    int height{0};
    int depth{0};

    /// Number of bytes to skip at the beginning of the file.
    qint64 headerBytes{0};

    /// Type of the voxels.
    VoxelType type{VoxelType::UInt8};

    /// Byte order of the voxels of 16 bits or more (false = little endian).
    bool bigEndian{false};

    /// Size in bytes the file must have for these parameters.
    qint64 expectedFileSize() const
    {
        return headerBytes + static_cast<qint64>(width) * height * depth * bytesPerVoxel(type);
    }
};

/**
 * @brief Guesses the sizes of a raw volume from its file name.
 *
 * Recognizes names like "tooth_103x94x161_uint8.raw" (width x height x depth). The voxel type
 * is also read from the name when it contains uint8, uint16, int16 or float (uint8 otherwise).
 *
 * @return The parameters found, with no header, or std::nullopt if the name does not contain
 *         the sizes.
 */
std::optional<RawParameters> guessRawParameters(const QString& fileName);

/**
 * @brief Tells whether the file name announces a voxel type that is not supported
 *        (32 or 64-bit integers, double).
 */
bool nameSuggestsUnsupportedVoxelType(const QString& fileName);

/**
 * @brief Loads a raw volume.
 *
 * The file size must match the parameters exactly, which catches most mistakes on the
 * sizes and on the voxel type. When the size would match another voxel type, the error says so.
 *
 * @param path Raw file.
 * @param parameters Sizes, voxel type, byte order and header of the file.
 * @param progress Optional progress and cancellation callback (called after each slice).
 */
Result loadRaw(const QString& path, const RawParameters& parameters,
               const ProgressCallback& progress = {});

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
