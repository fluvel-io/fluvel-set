// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "mean_filter_sliding.hpp"
#include "image_alpha.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace fluvel_ip::filter
{

void meanSliding(const ImageView& input, ImageOwner& output, int radius)
{
    MeanSliding filter;
    filter.apply(input, radius);
    std::swap(output, filter.outputRef());
}

ImageOwner meanSliding(const ImageView& input, int radius)
{
    MeanSliding filter;
    filter.apply(input, radius);
    return filter.output();
}

void MeanSliding::reset(const ImageView& input)
{
    if (!buffer1_.hasSameLayout(input))
    {
        buffer1_ = ImageOwner::like(input);
        buffer2_ = ImageOwner::like(input);
    }

    width_ = input.width();
    height_ = input.height();
}

void MeanSliding::apply(const ImageView& input, int radius)
{
    assert(radius >= 1);

    reset(input);

    const int channels = input.channels();
    const int activeChannels = std::min(channels, 3);

    // The borders are replicated (all the indices below are clamped), so any radius
    // is valid, even larger than the image.
    const int64_t kernelSize = 2 * static_cast<int64_t>(radius) + 1;

    // Ranges of the three loops of the horizontal pass: border, fast path, border.
    // They never overlap, even when the kernel is larger than the image.
    const int leftEndX = std::min(radius, width_);
    const int centerEndX = std::max(leftEndX, width_ - radius - 1);

    // =========================
    // PASS 1 — Horizontal (sliding + fast path)
    // =========================
    for (int y = 0; y < height_; ++y)
    {
        const uint8_t* src = input.row(y);
        uint8_t* dst = buffer1_.rowPtr(y);

        for (int c = 0; c < activeChannels; ++c)
        {
            int64_t sum = 0;

            // init window (clamping is only needed here)
            for (int k = -radius; k <= radius; ++k)
            {
                const int xx = std::clamp(k, 0, width_ - 1);
                sum += src[xx * channels + c];
            }

            // -------- LEFT BORDER --------
            for (int x = 0; x < leftEndX; ++x)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int xOut = std::clamp(x - radius, 0, width_ - 1);
                const int xIn = std::clamp(x + radius + 1, 0, width_ - 1);

                sum -= src[xOut * channels + c];
                sum += src[xIn * channels + c];
            }

            // -------- CENTER (FAST PATH) --------
            for (int x = leftEndX; x < centerEndX; ++x)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int xOut = x - radius;
                const int xIn = x + radius + 1;

                sum -= src[xOut * channels + c];
                sum += src[xIn * channels + c];
            }

            // -------- RIGHT BORDER --------
            for (int x = centerEndX; x < width_; ++x)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int xOut = std::clamp(x - radius, 0, width_ - 1);
                const int xIn = std::clamp(x + radius + 1, 0, width_ - 1);

                sum -= src[xOut * channels + c];
                sum += src[xIn * channels + c];
            }
        }
    }

    // =========================
    // PASS 2 — Vertical (sliding, row by row)
    // =========================
    // One running sum per column, so that the memory is read row by row (a column-by-column
    // walk touches one cache line per row, which is slow on large images).
    // The rows outside the image are replaced by the nearest border row.
    const int rowBytes = width_ * channels;
    std::vector<int64_t> sums(rowBytes, 0);

    // init window
    for (int k = -radius; k <= radius; ++k)
    {
        const uint8_t* row = buffer1_.rowPtr(std::clamp(k, 0, height_ - 1));

        for (int x = 0; x < width_; ++x)
            for (int c = 0; c < activeChannels; ++c)
                sums[x * channels + c] += row[x * channels + c];
    }

    for (int y = 0; y < height_; ++y)
    {
        uint8_t* dst = buffer2_.rowPtr(y);

        for (int x = 0; x < width_; ++x)
            for (int c = 0; c < activeChannels; ++c)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sums[idx] + kernelSize / 2) / kernelSize);
            }

        const uint8_t* rowOut = buffer1_.rowPtr(std::clamp(y - radius, 0, height_ - 1));
        const uint8_t* rowIn = buffer1_.rowPtr(std::clamp(y + radius + 1, 0, height_ - 1));

        for (int x = 0; x < width_; ++x)
            for (int c = 0; c < activeChannels; ++c)
            {
                const int idx = x * channels + c;
                sums[idx] += rowIn[idx] - rowOut[idx];
            }
    }

    std::swap(buffer1_, buffer2_);

    copyAlpha(input, buffer1_);
}

ImageView MeanSliding::outputView() const noexcept
{
    return buffer1_.view();
}

const ImageOwner& MeanSliding::output() const noexcept
{
    return buffer1_;
}

ImageOwner& MeanSliding::outputRef() noexcept
{
    return buffer1_;
}

} // namespace fluvel_ip::filter
