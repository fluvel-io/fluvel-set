// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_view.hpp"

#include <array>
#include <cstdint>

namespace fluvel_ip
{

/**
 * @brief Number of levels of an 8-bit channel.
 */
inline constexpr int kHistogramLevels = 256;

/**
 * @brief Intensity histogram of an 8-bit image.
 *
 * For a grayscale image only `bins[0]` is filled (gray level).
 * For a color image the channels are always stored in the order red, green, blue,
 * whatever the memory layout of the image (RGB, BGR, ...). The alpha channel is ignored.
 */
struct Histogram
{
    /// True for a color image (3 channels filled), false for grayscale (1 channel).
    bool color{false};

    /// Number of counted pixels (all the pixels, or the sampled ones, see computeHistogram()).
    uint64_t samples{0};

    /// bins[channel][level]: number of pixels of the channel having this level.
    std::array<std::array<uint64_t, kHistogramLevels>, 3> bins{};

    /// Number of channels filled (1 or 3).
    int channelCount() const noexcept
    {
        return color ? 3 : 1;
    }
};

/**
 * @brief Computes the histogram of an image.
 *
 * @param image Input image (Gray8, Rgb24, Bgr24, Bgr32 or Rgba32).
 * @param step Sampling step, in pixels, along both axes (1 = every pixel).
 *        A step larger than 1 gives an approximate histogram much faster on large images;
 *        its shape is preserved, only the counts are divided by about step * step.
 *
 * @return The histogram, or an empty one (no sample) if the image is empty or its format unknown.
 */
Histogram computeHistogram(const ImageView& image, int step = 1);

} // namespace fluvel_ip
