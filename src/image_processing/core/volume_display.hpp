// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_format.hpp"
#include "image_owner.hpp"
#include "volume_view.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace fluvel_ip
{

/**
 * @brief Range of voxel values shown on screen (window / level).
 *
 * Values <= low are displayed black and values >= high white, linearly in between.
 */
struct VoxelWindow
{
    double low{0.0};
    double high{255.0};
};

namespace detail
{

/// Linear mapping of a value to [0, 255]. NaN is mapped to 0.
inline std::uint8_t mapToGray8(double value, double low, double scale) noexcept
{
    const double t = (value - low) * scale;

    if (!(t > 0.0))
        return 0;

    if (t >= 255.0)
        return 255;

    return static_cast<std::uint8_t>(t + 0.5);
}

} // namespace detail

/**
 * @brief Smallest and largest voxel values of a volume (NaN values are ignored).
 *
 * @return (0, 0) for an empty volume.
 */
template <Voxel T>
[[nodiscard]]
std::pair<double, double> valueRange(const VolumeView<T>& volume)
{
    if (volume.isEmpty())
        return {0.0, 0.0};

    T lowest = volume.at(0, 0, 0);
    T highest = lowest;

    for (int z = 0; z < volume.depth(); ++z)
    {
        for (int y = 0; y < volume.height(); ++y)
        {
            const T* row = volume.data() + static_cast<std::ptrdiff_t>(z) * volume.sliceStride() +
                           static_cast<std::ptrdiff_t>(y) * volume.rowStride();

            for (int x = 0; x < volume.width(); ++x)
            {
                lowest = std::min(lowest, row[x]);
                highest = std::max(highest, row[x]);
            }
        }
    }

    return {static_cast<double>(lowest), static_cast<double>(highest)};
}

/**
 * @brief Slice of a volume converted to an 8-bit image through a window.
 *
 * Works for every voxel type. When the window is empty (high <= low) the image is black.
 *
 * @param volume Non-empty volume.
 * @param axis Cutting axis.
 * @param index Slice index along @p axis.
 * @param window Range of values mapped to [0, 255].
 *
 * @pre !volume.isEmpty() and 0 <= index < volume.sliceCount(axis)
 */
template <Voxel T>
[[nodiscard]]
ImageOwner sliceToGray8(const VolumeView<T>& volume, VolumeAxis axis, int index,
                        const VoxelWindow& window)
{
    assert(!volume.isEmpty());
    assert(index >= 0 && index < volume.sliceCount(axis));

    const double scale = window.high > window.low ? 255.0 / (window.high - window.low) : 0.0;

    int outWidth = volume.width();
    int outHeight = volume.height();

    switch (axis)
    {
        case VolumeAxis::Axial:
            break;
        case VolumeAxis::Coronal:
            outHeight = volume.depth();
            break;
        case VolumeAxis::Sagittal:
            outWidth = volume.height();
            outHeight = volume.depth();
            break;
    }

    ImageOwner out(outWidth, outHeight, ImageFormat::Gray8);

    for (int j = 0; j < outHeight; ++j)
    {
        std::uint8_t* dst = out.rowPtr(j);

        for (int i = 0; i < outWidth; ++i)
        {
            T value{};

            switch (axis)
            {
                case VolumeAxis::Axial:
                    value = volume.at(i, j, index);
                    break;
                case VolumeAxis::Coronal:
                    value = volume.at(i, index, j);
                    break;
                case VolumeAxis::Sagittal:
                    value = volume.at(index, i, j);
                    break;
            }

            dst[i] = detail::mapToGray8(static_cast<double>(value), window.low, scale);
        }
    }

    return out;
}

} // namespace fluvel_ip
