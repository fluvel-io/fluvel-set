// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_view.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fluvel_ip
{

/**
 * @brief Owning grayscale volume stored in contiguous memory.
 *
 * @tparam T Voxel type (see Voxel).
 *
 * The volume is tightly packed: rowStride = width and sliceStride = width * height.
 * Use view() to give it to the algorithms.
 */
template <Voxel T>
class VolumeOwner
{
public:
    /// Default constructor (empty volume).
    VolumeOwner() = default;

    /**
     * @brief Construct a volume filled with @p fill.
     *
     * @pre All the sizes are strictly positive.
     */
    VolumeOwner(int widthVoxels, int heightVoxels, int depthVoxels, T fill = T{},
                VoxelSpacing voxelSpacing = {})
        : width_(widthVoxels)
        , height_(heightVoxels)
        , depth_(depthVoxels)
        , spacing_(voxelSpacing)
        , data_(static_cast<std::size_t>(widthVoxels) * static_cast<std::size_t>(heightVoxels) *
                    static_cast<std::size_t>(depthVoxels),
                fill)
    {
        assert(widthVoxels > 0 && heightVoxels > 0 && depthVoxels > 0);
    }

    [[nodiscard]]
    bool isEmpty() const noexcept
    {
        return data_.empty();
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

    [[nodiscard]]
    const VoxelSpacing& spacing() const noexcept
    {
        return spacing_;
    }

    void setSpacing(const VoxelSpacing& voxelSpacing) noexcept
    {
        spacing_ = voxelSpacing;
    }

    /// Access a voxel (mutable).
    T& at(int x, int y, int z) noexcept
    {
        assert(x >= 0 && x < width_);
        assert(y >= 0 && y < height_);
        assert(z >= 0 && z < depth_);

        return data_[(static_cast<std::size_t>(z) * static_cast<std::size_t>(height_) +
                      static_cast<std::size_t>(y)) *
                         static_cast<std::size_t>(width_) +
                     static_cast<std::size_t>(x)];
    }

    /// Access a voxel (const).
    [[nodiscard]]
    T at(int x, int y, int z) const noexcept
    {
        return view().at(x, y, z);
    }

    /**
     * @brief Pointer to the first voxel of the axial slice @p z (mutable).
     *
     * The slice holds width() * height() tightly packed voxels, row by row.
     */
    T* sliceData(int z) noexcept
    {
        assert(z >= 0 && z < depth_);

        return data_.data() + static_cast<std::size_t>(z) * static_cast<std::size_t>(width_) *
                                  static_cast<std::size_t>(height_);
    }

    /// Non-owning view over the volume (empty view if the volume is empty).
    [[nodiscard]]
    VolumeView<T> view() const noexcept
    {
        if (data_.empty())
            return {};

        return VolumeView<T>(data_.data(), width_, height_, depth_, 0, 0, spacing_);
    }

private:
    int width_{0};
    int height_{0};
    int depth_{0};
    VoxelSpacing spacing_;
    std::vector<T> data_;
};

} // namespace fluvel_ip
