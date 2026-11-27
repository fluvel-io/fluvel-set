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
 * @brief Owning 8-bit grayscale volume stored in contiguous memory.
 *
 * The volume is tightly packed: rowStride = width and sliceStride = width * height.
 * Use view() to give it to the algorithms.
 */
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
    VolumeOwner(int widthVoxels, int heightVoxels, int depthVoxels, std::uint8_t fill = 0)
        : width_(widthVoxels)
        , height_(heightVoxels)
        , depth_(depthVoxels)
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

    /// Access a voxel (mutable).
    std::uint8_t& at(int x, int y, int z) noexcept
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
    std::uint8_t at(int x, int y, int z) const noexcept
    {
        return view().at(x, y, z);
    }

    /// Non-owning view over the volume (empty view if the volume is empty).
    [[nodiscard]]
    VolumeView view() const noexcept
    {
        if (data_.empty())
            return {};

        return VolumeView(data_.data(), width_, height_, depth_);
    }

private:
    int width_{0};
    int height_{0};
    int depth_{0};
    std::vector<std::uint8_t> data_;
};

} // namespace fluvel_ip
