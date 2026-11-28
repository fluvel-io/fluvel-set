// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "any_volume.hpp"

namespace fluvel
{

int bytesPerVoxel(VoxelType type) noexcept
{
    switch (type)
    {
        case VoxelType::UInt8:
            return 1;
        case VoxelType::UInt16:
        case VoxelType::Int16:
            return 2;
        case VoxelType::Float32:
            return 4;
    }

    return 1;
}

VoxelType AnyVolume::type() const noexcept
{
    // The order of the alternatives of Storage gives the type.
    switch (storage_.index())
    {
        case 0:
            return VoxelType::UInt8;
        case 1:
            return VoxelType::UInt16;
        case 2:
            return VoxelType::Int16;
        default:
            return VoxelType::Float32;
    }
}

bool AnyVolume::isEmpty() const noexcept
{
    return visit(
        [](const auto& v)
        {
            return v.isEmpty();
        });
}

int AnyVolume::width() const noexcept
{
    return visit(
        [](const auto& v)
        {
            return v.width();
        });
}

int AnyVolume::height() const noexcept
{
    return visit(
        [](const auto& v)
        {
            return v.height();
        });
}

int AnyVolume::depth() const noexcept
{
    return visit(
        [](const auto& v)
        {
            return v.depth();
        });
}

const fluvel_ip::VoxelSpacing& AnyVolume::spacing() const noexcept
{
    return visit(
        [](const auto& v) -> const fluvel_ip::VoxelSpacing&
        {
            return v.spacing();
        });
}

int AnyVolume::sliceCount(fluvel_ip::VolumeAxis axis) const noexcept
{
    return visit(
        [axis](const auto& v)
        {
            return v.view().sliceCount(axis);
        });
}

fluvel_ip::VoxelWindow AnyVolume::defaultWindow() const noexcept
{
    if (type() == VoxelType::UInt8)
        return {0.0, 255.0};

    return {range_.first, range_.second};
}

fluvel_ip::ImageOwner AnyVolume::sliceToGray8(fluvel_ip::VolumeAxis axis, int index,
                                              const fluvel_ip::VoxelWindow& window) const
{
    return visit(
        [&](const auto& v)
        {
            return fluvel_ip::sliceToGray8(v.view(), axis, index, window);
        });
}

} // namespace fluvel
