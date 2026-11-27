// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "region_color_speed_model.hpp"
#include "contour_data.hpp"
#include "contour_diagnostics.hpp"
#include "contour_types.hpp"
#include "fluvel_math.hpp"

#include <cassert>

namespace fluvel_ip
{

namespace
{

/// Sum over the components of weight * (a - b)^2, computed on 64 bits.
///
/// With 64 bits, the sum cannot overflow whatever the color space: at most
/// kMaxWeight * 3 * (a few hundreds)^2.
int64_t weightedSquaredDistance(const Color_3i& weights, const Color_3i& a, const Color_3i& b)
{
    const auto term = [](int weight, int ca, int cb)
    { return int64_t{weight} * math::square(int64_t{ca} - cb); };

    return term(weights.c1, a.c1, b.c1) + term(weights.c2, a.c2, b.c2) +
           term(weights.c3, a.c3, b.c3);
}

} // namespace

RegionColorSpeedModel::RegionColorSpeedModel(const RegionColorParams& params)
    : params_(params)
{
}

void RegionColorSpeedModel::onImageChanged(const ImageView& image, const ContourData& contour)
{
    assert(image.format() != ImageFormat::Gray8);

    initialMeansChecked_ = false;
    status_ = SpeedModelStatus::Ok;

    image_ = image;
    pixelCountTotal_ = image_.pixelCount();

    sumTotal_ = {0, 0, 0};

    sumOutside_ = {0, 0, 0};
    pixelCountOutside_ = 0;

    const auto& phi = contour.phi();

    for (int y = 0; y < phi.height(); ++y)
    {
        for (int x = 0; x < phi.width(); ++x)
        {
            const Rgb_64i rgb = static_cast<Rgb_64i>(image_.atPixelRgb(x, y));

            sumTotal_ += rgb;

            if (phi_value::isOutside(phi.at(x, y)))
            {
                sumOutside_ += rgb;
                ++pixelCountOutside_;
            }
        }
    }
}

void RegionColorSpeedModel::onStepCycle1()
{
    if (status_ != SpeedModelStatus::Ok)
        return;

    if (pixelCountOutside_ >= 1)
    {
        const Rgb<float> rgb = sumOutside_ / pixelCountOutside_;
        meanRgbOutside_ = rgb.rounded<unsigned char>();
        meanColorOut_ = rgbToColor(meanRgbOutside_);
    }
    else
    {
        status_ = SpeedModelStatus::RuntimeFailure;
    }

    const Rgb_64i sumIn = sumTotal_ - sumOutside_;
    const int64_t pxlNbrIn = pixelCountTotal_ - pixelCountOutside_;

    if (pxlNbrIn >= 1)
    {
        const Rgb<float> rgb = sumIn / pxlNbrIn;
        meanRgbInside_ = rgb.rounded<unsigned char>();
        meanColorIn_ = rgbToColor(meanRgbInside_);
    }
    else
    {
        status_ = SpeedModelStatus::RuntimeFailure;
    }

    if (!initialMeansChecked_)
    {
        if (status_ == SpeedModelStatus::Ok)
        {
            if (meanRgbOutside_ == meanRgbInside_)
                status_ = SpeedModelStatus::InitFailed;

            initialMeansChecked_ = true;
        }
    }
}

Color_3i RegionColorSpeedModel::rgbToColor(const Rgb_uc& rgb) const
{
    switch (params_.colorSpace)
    {
        case ColorSpaceOption::YUV:
            return color::rgbToYuv(rgb);

        case ColorSpaceOption::Lab:
        {
            const auto col = color::rgbToLab(rgb);
            return roundComponents(col.L, col.a, col.b);
        }

        case ColorSpaceOption::Luv:
        {
            const auto col = color::rgbToLuv(rgb);
            return roundComponents(col.L, col.u, col.v);
        }

        case ColorSpaceOption::RGB:
            return {static_cast<int>(rgb.red), static_cast<int>(rgb.green),
                    static_cast<int>(rgb.blue)};
    }

    std::unreachable();
}

void RegionColorSpeedModel::computeSpeed(ContourPoint& point, const DiscreteLevelSet&)
{
    const Rgb_uc rgb = image_.atPixelRgb(point.x(), point.y());

    const auto col = rgbToColor(rgb);

    // Only the sign of the speed is used. The weights are at most
    // RegionParams::kMaxWeight: with 64 bits the sum below stays far from the
    // limit, even for the extreme values of Lab and Luv.
    const int64_t lambdaOut = params_.lambdaOut;
    const int64_t lambdaIn = params_.lambdaIn;

    const int64_t speedOut = weightedSquaredDistance(params_.weights, col, meanColorOut_);
    const int64_t speedIn = weightedSquaredDistance(params_.weights, col, meanColorIn_);

    point.setSpeed(speed_value::getDiscreteSpeed(lambdaOut * speedOut - lambdaIn * speedIn));
}

void RegionColorSpeedModel::onSwitch(const ContourPoint& point, SwitchDirection direction)
{
    const Rgb_64i rgb = static_cast<Rgb_64i>(image_.atPixelRgb(point.x(), point.y()));

    if (direction == SwitchDirection::In)
    {
        sumOutside_ -= rgb;
        --pixelCountOutside_;
    }
    else if (direction == SwitchDirection::Out)
    {
        sumOutside_ += rgb;
        ++pixelCountOutside_;
    }
}

void RegionColorSpeedModel::fillDiagnostics(ContourDiagnostics& d) const
{
    d.meanInside =
        ChannelVector(static_cast<int>(meanRgbInside_.red), static_cast<int>(meanRgbInside_.green),
                      static_cast<int>(meanRgbInside_.blue));

    d.meanOutside = ChannelVector(static_cast<int>(meanRgbOutside_.red),
                                  static_cast<int>(meanRgbOutside_.green),
                                  static_cast<int>(meanRgbOutside_.blue));
}

} // namespace fluvel_ip
