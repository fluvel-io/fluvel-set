// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_format.hpp"
#include "image_owner.hpp"
#include "image_view.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace fluvel_ip
{

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
 * @brief Non-owning view over an 8-bit grayscale volume.
 *
 * Voxel (x, y, z) is stored at z * sliceStride + y * rowStride + x. Voxels are assumed
 * isotropic (the spacing is not handled yet).
 *
 * Axial and coronal slices are exposed as ImageView without any copy, so the 2D algorithms
 * can be run directly on them. A sagittal slice is not contiguous in memory and is therefore
 * copied into an ImageOwner.
 *
 * @warning The user is responsible for ensuring the lifetime of the underlying data.
 */
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
     * @param rowStrideBytes Bytes between two rows (0 = tightly packed).
     * @param sliceStrideBytes Bytes between two axial slices (0 = tightly packed).
     *
     * @pre data != nullptr
     * @pre All the sizes are strictly positive.
     */
    VolumeView(const std::uint8_t* data, int widthVoxels, int heightVoxels, int depthVoxels,
               int rowStrideBytes = 0, int sliceStrideBytes = 0)
        : data_(data)
        , width_(widthVoxels)
        , height_(heightVoxels)
        , depth_(depthVoxels)
        , rowStride_(rowStrideBytes == 0 ? widthVoxels : rowStrideBytes)
        , sliceStride_(sliceStrideBytes == 0 ? rowStride_ * heightVoxels : sliceStrideBytes)
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
    const std::uint8_t* data() const noexcept
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

    /// Bytes between two rows.
    [[nodiscard]]
    int rowStride() const noexcept
    {
        return rowStride_;
    }

    /// Bytes between two axial slices.
    [[nodiscard]]
    int sliceStride() const noexcept
    {
        return sliceStride_;
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
    std::uint8_t at(int x, int y, int z) const noexcept
    {
        assert(x >= 0 && x < width_);
        assert(y >= 0 && y < height_);
        assert(z >= 0 && z < depth_);

        return data_[static_cast<std::ptrdiff_t>(z) * sliceStride_ +
                     static_cast<std::ptrdiff_t>(y) * rowStride_ + x];
    }

    /**
     * @brief Axial slice (fixed z) as a view, without copy.
     *
     * @pre !isEmpty() and 0 <= z < depth()
     */
    [[nodiscard]]
    ImageView axialSlice(int z) const noexcept
    {
        assert(!isEmpty());
        assert(z >= 0 && z < depth_);

        return ImageView(data_ + static_cast<std::ptrdiff_t>(z) * sliceStride_, width_, height_,
                         ImageFormat::Gray8, rowStride_);
    }

    /**
     * @brief Coronal slice (fixed y) as a view, without copy.
     *
     * The rows of the image are the axial slices, hence the stride equals sliceStride().
     *
     * @pre !isEmpty() and 0 <= y < height()
     */
    [[nodiscard]]
    ImageView coronalSlice(int y) const noexcept
    {
        assert(!isEmpty());
        assert(y >= 0 && y < height_);

        return ImageView(data_ + static_cast<std::ptrdiff_t>(y) * rowStride_, width_, depth_,
                         ImageFormat::Gray8, sliceStride_);
    }

    /**
     * @brief Sagittal slice (fixed x), copied into a new image.
     *
     * @pre !isEmpty() and 0 <= x < width()
     */
    [[nodiscard]]
    ImageOwner sagittalSlice(int x) const
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
    const std::uint8_t* data_{nullptr};
    int width_{0};
    int height_{0};
    int depth_{0};
    int rowStride_{0};
    int sliceStride_{0};
};

} // namespace fluvel_ip
