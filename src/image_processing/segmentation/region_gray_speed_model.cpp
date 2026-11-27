// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "region_gray_speed_model.hpp"

#include "contour_data.hpp"
#include "contour_diagnostics.hpp"
#include "contour_types.hpp"
#include "fluvel_math.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fluvel_ip
{

// The speed is computed on int: the largest term is kMaxWeight * 255^2.
static_assert(int64_t{RegionParams::kMaxWeight} * 255 * 255 <= std::numeric_limits<int>::max(),
              "the gray speed must fit in an int");

RegionGraySpeedModel::RegionGraySpeedModel(
    const RegionParams& params) /* optional parameter with RegionParams() */
    : params_(params)
{
}

void RegionGraySpeedModel::onImageChanged(const ImageView& image, const ContourData& contour)
{
    assert(image.format() == ImageFormat::Gray8);

    initialMeansChecked_ = false;
    status_ = SpeedModelStatus::Ok;

    image_ = image;
    pixelCountTotal_ = image_.pixelCount();

    sumTotal_ = 0;

    sumOutside_ = 0;
    pixelCountOutside_ = 0;

    const auto& phi = contour.phi();

    for (int y = 0; y < phi.height(); ++y)
    {
        for (int x = 0; x < phi.width(); ++x)
        {
            const int64_t intensity = static_cast<int64_t>(image_.at(x, y));

            sumTotal_ += intensity;

            if (phi_value::isOutside(phi.at(x, y)))
            {
                sumOutside_ += intensity;
                ++pixelCountOutside_;
            }
        }
    }
}

void RegionGraySpeedModel::onStepCycle1()
{
    if (status_ != SpeedModelStatus::Ok)
        return;

    if (pixelCountOutside_ >= 1)
        meanOutside_ = std::lround(static_cast<float>(sumOutside_) / pixelCountOutside_);
    else
        status_ = SpeedModelStatus::RuntimeFailure;

    const int64_t sumInside = sumTotal_ - sumOutside_;
    const int64_t pixelCountInside = pixelCountTotal_ - pixelCountOutside_;

    if (pixelCountInside >= 1)
        meanInside_ = std::lround(static_cast<float>(sumInside) / pixelCountInside);
    else
        status_ = SpeedModelStatus::RuntimeFailure;

    if (!initialMeansChecked_)
    {
        if (status_ == SpeedModelStatus::Ok)
        {
            if (meanOutside_ == meanInside_)
                status_ = SpeedModelStatus::InitFailed;

            initialMeansChecked_ = true;
        }
    }
}

void RegionGraySpeedModel::computeSpeed(ContourPoint& point, const DiscreteLevelSet&)
{
    const int pxl = static_cast<int>(image_.at(point.x(), point.y()));

    const int diffOut = pxl - meanOutside_;
    const int diffIn = pxl - meanInside_;

    // The weights are at most RegionParams::kMaxWeight and a difference is at
    // most 255: the speed fits in an int (checked at the top of this file).
    const int speed =
        params_.lambdaOut * (math::square(diffOut)) - params_.lambdaIn * (math::square(diffIn));

    point.setSpeed(speed_value::getDiscreteSpeed(speed));
}

void RegionGraySpeedModel::onSwitch(const ContourPoint& point, SwitchDirection direction)
{
    const int64_t intensity = static_cast<int64_t>(image_.at(point.x(), point.y()));

    if (direction == SwitchDirection::In)
    {
        sumOutside_ -= intensity;
        --pixelCountOutside_;
    }
    else if (direction == SwitchDirection::Out)
    {
        sumOutside_ += intensity;
        ++pixelCountOutside_;
    }
}

void RegionGraySpeedModel::fillDiagnostics(ContourDiagnostics& d) const
{
    d.meanInside = ChannelVector(meanInside_);
    d.meanOutside = ChannelVector(meanOutside_);
}

} // namespace fluvel_ip
