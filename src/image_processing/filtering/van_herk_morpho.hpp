// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_alpha.hpp"
#include "image_owner.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>

namespace fluvel_ip::filter::morpho
{

namespace detail
{

/**
 * @brief Horizontal pass of the Van Herk algorithm.
 *
 * Computes sliding-window min/max (depending on IsMax) along image rows
 * using a monotonic deque for O(N) complexity.
 *
 * @tparam IsMax If true computes dilation (max), otherwise erosion (min).
 * @param src Pointer to input row data.
 * @param dst Pointer to output row data.
 * @param width Image width.
 * @param channels Number of channels per pixel (1, 3 or 4).
 * @param activeChannels Number of processed channels (1 or 3).
 * @param radius Radius of the structuring element.
 */
template <bool IsMax>
void vanHerkHorizontal(const uint8_t* src, uint8_t* dst, int width, int channels,
                       int activeChannels, int radius)
{
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        std::deque<int> dq;

        // Next input index to push in the window.
        int next = 0;

        for (int out = 0; out < width; ++out)
        {
            // Window of the output pixel, clipped to the image: [out - radius, out + radius].
            // (written as differences: no overflow, whatever the radius)
            while (next < width && next - out <= radius)
            {
                const uint8_t b = src[next * channels + ch];

                // Maintain monotonic deque
                while (!dq.empty())
                {
                    const uint8_t a = src[dq.back() * channels + ch];

                    if constexpr (IsMax)
                    {
                        if (a <= b)
                            dq.pop_back();
                        else
                            break;
                    }
                    else
                    {
                        if (a >= b)
                            dq.pop_back();
                        else
                            break;
                    }
                }

                dq.push_back(next);
                ++next;
            }

            // Remove indices left of the window
            while (out - dq.front() > radius)
                dq.pop_front();

            dst[out * channels + ch] = src[dq.front() * channels + ch];
        }
    }
}

/**
 * @brief Vertical pass of the Van Herk algorithm.
 *
 * Applies the same sliding-window logic as the horizontal pass,
 * but along image columns.
 *
 * @tparam IsMax If true computes dilation (max), otherwise erosion (min).
 * @param buffer Intermediate buffer from horizontal pass.
 * @param output Output image.
 * @param width Image width.
 * @param height Image height.
 * @param channels Number of channels per pixel (1, 3 or 4).
 * @param activeChannels Number of processed channels (1 or 3).
 * @param radius Radius of the structuring element.
 */
template <bool IsMax>
void vanHerkVertical(const ImageOwner& buffer, ImageOwner& output, int width, int height,
                     int channels, int activeChannels, int radius)
{
    for (int x = 0; x < width; ++x)
    {
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            const int idx = x * channels + ch;

            std::deque<int> dq;

            // Next input row to push in the window.
            int next = 0;

            for (int out = 0; out < height; ++out)
            {
                // Window of the output row, clipped to the image: [out - radius, out + radius].
                while (next < height && next - out <= radius)
                {
                    const uint8_t b = buffer.rowPtr(next)[idx];

                    while (!dq.empty())
                    {
                        const uint8_t a = buffer.rowPtr(dq.back())[idx];

                        if constexpr (IsMax)
                        {
                            if (a <= b)
                                dq.pop_back();
                            else
                                break;
                        }
                        else
                        {
                            if (a >= b)
                                dq.pop_back();
                            else
                                break;
                        }
                    }

                    dq.push_back(next);
                    ++next;
                }

                while (out - dq.front() > radius)
                    dq.pop_front();

                output.rowPtr(out)[idx] = buffer.rowPtr(dq.front())[idx];
            }
        }
    }
}

} // namespace detail

/**
 * @brief Morphological filter using the Van Herk algorithm.
 *
 * Computes grayscale dilation (max) or erosion (min) with a square
 * structuring element of size (2 * radius + 1).
 *
 * This implementation achieves O(N) complexity per row/column using
 * a sliding-window monotonic deque.
 *
 * The algorithm is applied in two separable passes:
 * - horizontal
 * - vertical
 *
 * @tparam IsMax If true computes dilation, otherwise erosion.
 * @param input Input image.
 * @param output Output image (must have same layout as input).
 * @param radius Radius of the structuring element.
 */
template <bool IsMax>
void vanHerk(const ImageView& input, ImageOwner& output, int radius)
{
    output.ensureLike(input);

    assert(output.width() == input.width());
    assert(output.height() == input.height());
    assert(output.format() == input.format());
    assert(radius >= 1);

    const int w = input.width();
    const int h = input.height();

    const int channels = input.channels();
    const int activeChannels = std::min(channels, 3);

    // Intermediate buffer
    ImageOwner buffer(w, h, input.format());

    // Horizontal pass
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src = input.row(y);
        uint8_t* dst = buffer.rowPtr(y);

        detail::vanHerkHorizontal<IsMax>(src, dst, w, channels, activeChannels, radius);
    }

    // Vertical pass
    detail::vanHerkVertical<IsMax>(buffer, output, w, h, channels, activeChannels, radius);

    copyAlpha(input, output);
}

} // namespace fluvel_ip::filter::morpho
