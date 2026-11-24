// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "image_alpha.hpp"
#include "image_owner.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace fluvel_ip::filter::morpho
{

namespace detail
{

/**
 * @brief Sliding-window min/max over one line of samples.
 *
 * Monotonic deque (stored in a ring buffer) giving O(N) complexity, whatever the radius.
 * The window of each output sample is [i - radius, i + radius], clipped to the line.
 *
 * @tparam IsMax If true computes dilation (max), otherwise erosion (min).
 * @param src First input sample.
 * @param srcStride Distance between two input samples (in bytes).
 * @param dst First output sample.
 * @param dstStride Distance between two output samples (in bytes).
 * @param n Number of samples.
 * @param radius Radius of the window.
 * @param ring Scratch buffer, reused from one call to the next to avoid reallocations.
 */
template <bool IsMax>
void slidingExtremum(const uint8_t* src, int srcStride, uint8_t* dst, int dstStride, int n,
                     int radius, std::vector<int>& ring)
{
    // The deque holds at most the window (2 * radius + 1 samples) plus one expired index,
    // which is only removed after the new samples have been pushed.
    const int cap =
        static_cast<int>(std::min<int64_t>(2 * static_cast<int64_t>(radius) + 1, n)) + 1;

    if (static_cast<int>(ring.size()) < cap)
        ring.resize(cap);

    int head = 0;  // position of the front element
    int count = 0; // number of elements

    const auto wrap = [cap](int p)
    {
        return p >= cap ? p - cap : p;
    };

    // Next input index to push in the window.
    int next = 0;

    for (int out = 0; out < n; ++out)
    {
        // Window of the output sample, clipped to the line: [out - radius, out + radius].
        // (written as differences: no overflow, whatever the radius)
        while (next < n && next - out <= radius)
        {
            const uint8_t b = src[next * srcStride];

            // Maintain monotonic deque
            while (count > 0)
            {
                const uint8_t a = src[ring[wrap(head + count - 1)] * srcStride];

                if constexpr (IsMax)
                {
                    if (a <= b)
                        --count;
                    else
                        break;
                }
                else
                {
                    if (a >= b)
                        --count;
                    else
                        break;
                }
            }

            ring[wrap(head + count)] = next;
            ++count;
            ++next;
        }

        // Remove indices left of the window
        while (out - ring[head] > radius)
        {
            head = wrap(head + 1);
            --count;
        }

        dst[out * dstStride] = src[ring[head] * srcStride];
    }
}

/**
 * @brief Horizontal pass of the Van Herk algorithm.
 *
 * Computes sliding-window min/max (depending on IsMax) along image rows.
 *
 * @tparam IsMax If true computes dilation (max), otherwise erosion (min).
 * @param src Pointer to input row data.
 * @param dst Pointer to output row data.
 * @param width Image width.
 * @param channels Number of channels per pixel (1, 3 or 4).
 * @param activeChannels Number of processed channels (1 or 3).
 * @param radius Radius of the structuring element.
 * @param ring Scratch buffer shared by all the calls.
 */
template <bool IsMax>
void vanHerkHorizontal(const uint8_t* src, uint8_t* dst, int width, int channels,
                       int activeChannels, int radius, std::vector<int>& ring)
{
    for (int ch = 0; ch < activeChannels; ++ch)
        slidingExtremum<IsMax>(src + ch, channels, dst + ch, channels, width, radius, ring);
}

/**
 * @brief Vertical pass of the Van Herk algorithm.
 *
 * Same sliding window as the horizontal pass, along image columns.
 *
 * Walking down a column touches one cache line per row, which is slow on large images.
 * The columns are therefore processed by tiles: a tile is copied transposed (each column
 * becomes a contiguous line), filtered, then copied back. The result is the same.
 *
 * @tparam IsMax If true computes dilation (max), otherwise erosion (min).
 * @param buffer Intermediate buffer from horizontal pass.
 * @param output Output image.
 * @param width Image width.
 * @param height Image height.
 * @param channels Number of channels per pixel (1, 3 or 4).
 * @param activeChannels Number of processed channels (1 or 3).
 * @param radius Radius of the structuring element.
 * @param ring Scratch buffer shared by all the calls.
 */
template <bool IsMax>
void vanHerkVertical(const ImageOwner& buffer, ImageOwner& output, int width, int height,
                     int channels, int activeChannels, int radius, std::vector<int>& ring)
{
    // Pixels per tile: tile size = kTilePixels * activeChannels * height bytes (x2).
    constexpr int kTilePixels = 16;

    const size_t lineCount = static_cast<size_t>(kTilePixels) * activeChannels;
    std::vector<uint8_t> tileIn(lineCount * height);
    std::vector<uint8_t> tileOut(lineCount * height);

    for (int x0 = 0; x0 < width; x0 += kTilePixels)
    {
        const int tileWidth = std::min(kTilePixels, width - x0);
        const int lines = tileWidth * activeChannels;

        // Transpose: line j = (pixel, channel) of the tile, one sample per row.
        for (int y = 0; y < height; ++y)
        {
            const uint8_t* row = buffer.rowPtr(y) + x0 * channels;

            for (int px = 0; px < tileWidth; ++px)
                for (int ch = 0; ch < activeChannels; ++ch)
                    tileIn[static_cast<size_t>(px * activeChannels + ch) * height + y] =
                        row[px * channels + ch];
        }

        for (int j = 0; j < lines; ++j)
        {
            const size_t offset = static_cast<size_t>(j) * height;
            slidingExtremum<IsMax>(tileIn.data() + offset, 1, tileOut.data() + offset, 1, height,
                                   radius, ring);
        }

        // Transpose back.
        for (int y = 0; y < height; ++y)
        {
            uint8_t* row = output.rowPtr(y) + x0 * channels;

            for (int px = 0; px < tileWidth; ++px)
                for (int ch = 0; ch < activeChannels; ++ch)
                    row[px * channels + ch] =
                        tileOut[static_cast<size_t>(px * activeChannels + ch) * height + y];
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

    std::vector<int> ring;

    // Horizontal pass
    for (int y = 0; y < h; ++y)
    {
        const uint8_t* src = input.row(y);
        uint8_t* dst = buffer.rowPtr(y);

        detail::vanHerkHorizontal<IsMax>(src, dst, w, channels, activeChannels, radius, ring);
    }

    // Vertical pass
    detail::vanHerkVertical<IsMax>(buffer, output, w, h, channels, activeChannels, radius, ring);

    copyAlpha(input, output);
}

} // namespace fluvel_ip::filter::morpho
