// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "histogram.hpp"

#include <algorithm>

namespace fluvel_ip
{

Histogram computeHistogram(const ImageView& image, int step)
{
    Histogram histogram;

    const int width = image.width();
    const int height = image.height();

    if (width <= 0 || height <= 0 || image.format() == ImageFormat::Unknown)
        return histogram;

    step = std::max(step, 1);

    const int channels = image.channels();

    // Offset of the red, green and blue samples inside a pixel.
    int redOffset = 0;
    int greenOffset = 1;
    int blueOffset = 2;

    switch (image.format())
    {
        case ImageFormat::Gray8:
            histogram.color = false;
            break;

        case ImageFormat::Rgb24:
        case ImageFormat::Rgba32:
            histogram.color = true;
            break;

        case ImageFormat::Bgr24:
        case ImageFormat::Bgr32:
            histogram.color = true;
            redOffset = 2;
            blueOffset = 0;
            break;

        case ImageFormat::Unknown:
            return histogram;
    }

    for (int y = 0; y < height; y += step)
    {
        const uint8_t* row = image.row(y);

        if (histogram.color)
        {
            auto& red = histogram.bins[0];
            auto& green = histogram.bins[1];
            auto& blue = histogram.bins[2];

            for (int x = 0; x < width; x += step)
            {
                const uint8_t* pixel = row + x * channels;

                ++red[pixel[redOffset]];
                ++green[pixel[greenOffset]];
                ++blue[pixel[blueOffset]];
            }
        }
        else
        {
            auto& gray = histogram.bins[0];

            for (int x = 0; x < width; x += step)
                ++gray[row[x]];
        }

        histogram.samples += static_cast<uint64_t>((width + step - 1) / step);
    }

    return histogram;
}

} // namespace fluvel_ip
