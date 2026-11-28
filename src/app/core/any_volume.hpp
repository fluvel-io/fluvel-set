// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_owner.hpp"
#include "volume_display.hpp"
#include "volume_owner.hpp"

#include <cstdint>
#include <utility>
#include <variant>

namespace fluvel
{

/**
 * @brief Voxel types a loaded volume can have.
 */
enum class VoxelType
{
    UInt8,
    UInt16,
    Int16,
    Float32
};

/// Size in bytes of one voxel of the given type.
[[nodiscard]]
int bytesPerVoxel(VoxelType type) noexcept;

/**
 * @brief Volume whose voxel type is only known at run time (type erasure).
 *
 * The algorithms are written once as templates on fluvel_ip::VolumeView<T>. A volume read
 * from a file has a type given by the file, so the application holds it in an AnyVolume and
 * calls visit() at one place to reach the right instantiation.
 */
class AnyVolume
{
public:
    using Storage =
        std::variant<fluvel_ip::VolumeOwner<std::uint8_t>, fluvel_ip::VolumeOwner<std::uint16_t>,
                     fluvel_ip::VolumeOwner<std::int16_t>, fluvel_ip::VolumeOwner<float>>;

    /// Empty volume.
    AnyVolume() = default;

    /// Take the ownership of a typed volume.
    template <fluvel_ip::Voxel T>
    explicit AnyVolume(fluvel_ip::VolumeOwner<T> volume)
        : storage_(std::move(volume))
    {
        range_ = visit(
            [](const auto& v)
            {
                return fluvel_ip::valueRange(v.view());
            });
    }

    [[nodiscard]]
    VoxelType type() const noexcept;

    [[nodiscard]]
    bool isEmpty() const noexcept;

    [[nodiscard]]
    int width() const noexcept;

    [[nodiscard]]
    int height() const noexcept;

    [[nodiscard]]
    int depth() const noexcept;

    [[nodiscard]]
    const fluvel_ip::VoxelSpacing& spacing() const noexcept;

    /// Number of slices along @p axis (0 for an empty volume).
    [[nodiscard]]
    int sliceCount(fluvel_ip::VolumeAxis axis) const noexcept;

    /// Smallest and largest voxel values, computed once at construction.
    [[nodiscard]]
    std::pair<double, double> valueRange() const noexcept
    {
        return range_;
    }

    /**
     * @brief Window shown by default: 0..255 for 8 bits, the whole value range otherwise.
     */
    [[nodiscard]]
    fluvel_ip::VoxelWindow defaultWindow() const noexcept;

    /**
     * @brief Slice converted to an 8-bit image for display.
     *
     * @pre !isEmpty() and 0 <= index < sliceCount(axis)
     */
    [[nodiscard]]
    fluvel_ip::ImageOwner sliceToGray8(fluvel_ip::VolumeAxis axis, int index,
                                       const fluvel_ip::VoxelWindow& window) const;

    /**
     * @brief Call @p visitor with the typed volume (a fluvel_ip::VolumeOwner<T>).
     */
    template <typename Visitor>
    decltype(auto) visit(Visitor&& visitor) const
    {
        return std::visit(std::forward<Visitor>(visitor), storage_);
    }

private:
    Storage storage_;
    std::pair<double, double> range_{0.0, 0.0};
};

} // namespace fluvel
