// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_format.hpp"
#include "image_owner.hpp"
#include "image_view.hpp"

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace fluvel_ip
{

/**
 * @brief Types a voxel can have: 8 or 16 bits (signed or not) and 32-bit float.
 *
 * A 12-bit scan is stored in a 16-bit voxel. CT scans in Hounsfield units need int16_t.
 */
template <typename T>
concept Voxel = std::same_as<T, std::uint8_t> || std::same_as<T, std::uint16_t> ||
                std::same_as<T, std::int16_t> || std::same_as<T, float>;

/**
 * @brief Distance between two voxel centers along each axis (in millimeters when known).
 */
struct VoxelSpacing
{
    double x{1.0};
    double y{1.0};
    double z{1.0};

    bool operator==(const VoxelSpacing&) const = default;
};

/**
 * @brief Axes used to cut a volume into slices.
 */
enum class VolumeAxis
{
    Axial,   ///< Fixed z: the slice is a (width x height) image.
    Coronal, ///< Fixed y: the slice is a (width x depth) image.
    Sagittal ///< Fixed x: the slice is a (height x depth) image.
};

/**
 * @brief Non-owning view over a grayscale volume.
 *
 * @tparam T Voxel type (see Voxel).
 *
 * Voxel (x, y, z) is stored at z * sliceStride + y * rowStride + x. The strides are counted in
 * voxels, not in bytes. The spacing is stored with the view but not used by the slices.
 *
 * For 8-bit volumes, axial and coronal slices are exposed as ImageView without any copy, so
 * the 2D algorithms can be run directly on them. A sagittal slice is not contiguous in memory
 * and is therefore copied into an ImageOwner. Use sliceToGray8() to display any voxel type.
 *
 * @warning The user is responsible for ensuring the lifetime of the underlying data.
 */
template <Voxel T>
class VolumeView final
{
public:
    /// Default constructor (empty view).
    VolumeView() = default;

    /**
     * @brief Construct a volume view from raw data.
     *
     * @param data Pointer to the first voxel.
     * @param widthVoxels Size along x.
     * @param heightVoxels Size along y.
     * @param depthVoxels Size along z.
     * @param rowStrideVoxels Voxels between two rows (0 = tightly packed).
     * @param sliceStrideVoxels Voxels between two axial slices (0 = tightly packed).
     * @param voxelSpacing Distance between voxels.
     *
     * @pre data != nullptr
     * @pre All the sizes are strictly positive.
     */
    VolumeView(const T* data, int widthVoxels, int heightVoxels, int depthVoxels,
               std::ptrdiff_t rowStrideVoxels = 0, std::ptrdiff_t sliceStrideVoxels = 0,
               VoxelSpacing voxelSpacing = {})
        : data_(data)
        , width_(widthVoxels)
        , height_(heightVoxels)
        , depth_(depthVoxels)
        , rowStride_(rowStrideVoxels == 0 ? widthVoxels : rowStrideVoxels)
        , sliceStride_(sliceStrideVoxels == 0 ? rowStride_ * heightVoxels : sliceStrideVoxels)
        , spacing_(voxelSpacing)
    {
        assert(data != nullptr);
        assert(widthVoxels > 0 && heightVoxels > 0 && depthVoxels > 0);
        assert(rowStride_ >= width_);
        assert(sliceStride_ >= rowStride_ * height_);
    }

    [[nodiscard]]
    bool isEmpty() const noexcept
    {
        return data_ == nullptr;
    }

    [[nodiscard]]
    const T* data() const noexcept
    {
        return data_;
    }

    [[nodiscard]]
    int width() const noexcept
    {
        return width_;
    }

    [[nodiscard]]
    int height() const noexcept
    {
        return height_;
    }

    [[nodiscard]]
    int depth() const noexcept
    {
        return depth_;
    }

    /// Voxels between two rows.
    [[nodiscard]]
    std::ptrdiff_t rowStride() const noexcept
    {
        return rowStride_;
    }

    /// Voxels between two axial slices.
    [[nodiscard]]
    std::ptrdiff_t sliceStride() const noexcept
    {
        return sliceStride_;
    }

    [[nodiscard]]
    const VoxelSpacing& spacing() const noexcept
    {
        return spacing_;
    }

    /// Number of slices along @p axis (0 for an empty view).
    [[nodiscard]]
    int sliceCount(VolumeAxis axis) const noexcept
    {
        if (isEmpty())
            return 0;

        switch (axis)
        {
            case VolumeAxis::Axial:
                return depth_;
            case VolumeAxis::Coronal:
                return height_;
            case VolumeAxis::Sagittal:
                return width_;
        }

        return 0;
    }

    /// Get a voxel value.
    [[nodiscard]]
    T at(int x, int y, int z) const noexcept
    {
        assert(x >= 0 && x < width_);
        assert(y >= 0 && y < height_);
        assert(z >= 0 && z < depth_);

        return data_[static_cast<std::ptrdiff_t>(z) * sliceStride_ +
                     static_cast<std::ptrdiff_t>(y) * rowStride_ + x];
    }

    /**
     * @brief Axial slice (fixed z) as a view, without copy (8-bit volumes only).
     *
     * @pre !isEmpty() and 0 <= z < depth()
     */
    [[nodiscard]]
    ImageView axialSlice(int z) const noexcept
        requires std::same_as<T, std::uint8_t>
    {
        assert(!isEmpty());
        assert(z >= 0 && z < depth_);

        return ImageView(data_ + static_cast<std::ptrdiff_t>(z) * sliceStride_, width_, height_,
                         ImageFormat::Gray8, static_cast<int>(rowStride_));
    }

    /**
     * @brief Coronal slice (fixed y) as a view, without copy (8-bit volumes only).
     *
     * The rows of the image are the axial slices, hence the stride equals sliceStride().
     *
     * @pre !isEmpty() and 0 <= y < height()
     */
    [[nodiscard]]
    ImageView coronalSlice(int y) const noexcept
        requires std::same_as<T, std::uint8_t>
    {
        assert(!isEmpty());
        assert(y >= 0 && y < height_);

        return ImageView(data_ + static_cast<std::ptrdiff_t>(y) * rowStride_, width_, depth_,
                         ImageFormat::Gray8, static_cast<int>(sliceStride_));
    }

    /**
     * @brief Sagittal slice (fixed x), copied into a new image (8-bit volumes only).
     *
     * @pre !isEmpty() and 0 <= x < width()
     */
    [[nodiscard]]
    ImageOwner sagittalSlice(int x) const
        requires std::same_as<T, std::uint8_t>
    {
        assert(!isEmpty());
        assert(x >= 0 && x < width_);

        ImageOwner out(height_, depth_, ImageFormat::Gray8);

        for (int z = 0; z < depth_; ++z)
        {
            std::uint8_t* dst = out.rowPtr(z);

            for (int y = 0; y < height_; ++y)
                dst[y] = at(x, y, z);
        }

        return out;
    }

private:
    const T* data_{nullptr};
    int width_{0};
    int height_{0};
    int depth_{0};
    std::ptrdiff_t rowStride_{0};
    std::ptrdiff_t sliceStride_{0};
    VoxelSpacing spacing_;
};

} // namespace fluvel_ip
