// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "mean_filter_sliding.hpp"
#include "image_alpha.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

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

    // Ranges of the three loops of a pass: border, fast path, border.
    // They never overlap, even when the kernel is larger than the image.
    const int leftEndX = std::min(radius, width_);
    const int centerEndX = std::max(leftEndX, width_ - radius - 1);
    const int topEndY = std::min(radius, height_);
    const int centerEndY = std::max(topEndY, height_ - radius - 1);

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

            // init window (avec clamp uniquement ici)
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

                const int x_out = std::clamp(x - radius, 0, width_ - 1);
                const int x_in = std::clamp(x + radius + 1, 0, width_ - 1);

                sum -= src[x_out * channels + c];
                sum += src[x_in * channels + c];
            }

            // -------- CENTER (FAST PATH) --------
            for (int x = leftEndX; x < centerEndX; ++x)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int x_out = x - radius;
                const int x_in = x + radius + 1;

                sum -= src[x_out * channels + c];
                sum += src[x_in * channels + c];
            }

            // -------- RIGHT BORDER --------
            for (int x = centerEndX; x < width_; ++x)
            {
                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int x_out = std::clamp(x - radius, 0, width_ - 1);
                const int x_in = std::clamp(x + radius + 1, 0, width_ - 1);

                sum -= src[x_out * channels + c];
                sum += src[x_in * channels + c];
            }
        }
    }

    // =========================
    // PASS 2 — Vertical (sliding + fast path)
    // =========================
    for (int x = 0; x < width_; ++x)
    {
        for (int c = 0; c < activeChannels; ++c)
        {
            int64_t sum = 0;

            // init window
            for (int k = -radius; k <= radius; ++k)
            {
                const int yy = std::clamp(k, 0, height_ - 1);
                const uint8_t* row = buffer1_.rowPtr(yy);

                sum += row[x * channels + c];
            }

            // -------- TOP BORDER --------
            for (int y = 0; y < topEndY; ++y)
            {
                uint8_t* dst = buffer2_.rowPtr(y);

                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int y_out = std::clamp(y - radius, 0, height_ - 1);
                const int y_in = std::clamp(y + radius + 1, 0, height_ - 1);

                const uint8_t* row_out = buffer1_.rowPtr(y_out);
                const uint8_t* row_in = buffer1_.rowPtr(y_in);

                sum -= row_out[x * channels + c];
                sum += row_in[x * channels + c];
            }

            // -------- CENTER (FAST PATH) --------
            for (int y = topEndY; y < centerEndY; ++y)
            {
                uint8_t* dst = buffer2_.rowPtr(y);

                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int y_out = y - radius;
                const int y_in = y + radius + 1;

                const uint8_t* row_out = buffer1_.rowPtr(y_out);
                const uint8_t* row_in = buffer1_.rowPtr(y_in);

                sum -= row_out[x * channels + c];
                sum += row_in[x * channels + c];
            }

            // -------- BOTTOM BORDER --------
            for (int y = centerEndY; y < height_; ++y)
            {
                uint8_t* dst = buffer2_.rowPtr(y);

                const int idx = x * channels + c;
                dst[idx] = static_cast<uint8_t>((sum + kernelSize / 2) / kernelSize);

                const int y_out = std::clamp(y - radius, 0, height_ - 1);
                const int y_in = std::clamp(y + radius + 1, 0, height_ - 1);

                const uint8_t* row_out = buffer1_.rowPtr(y_out);
                const uint8_t* row_in = buffer1_.rowPtr(y_in);

                sum -= row_out[x * channels + c];
                sum += row_in[x * channels + c];
            }
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

} // namespace fluvel_ip
