// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "display_adjustment.hpp"

#include <QColor>

#include <algorithm>
#include <cmath>

namespace fluvel::display
{

namespace
{

/// Share of the pixels ignored at each end of the histogram by auto-contrast.
constexpr double kClippedFraction{0.01};

/// Approximate number of pixels read to measure the range.
constexpr qint64 kSampleCount{200000};

/// Below this width the range is considered flat: stretching it would only amplify noise.
constexpr int kMinimumRangeWidth{8};

/// Share of the new auto-contrast range taken at each frame of a stream (the rest comes from
/// the previous frames).
constexpr double kRangeSmoothing{0.2};

/// Contrast slope: 1 at 0, up to 5 at +100 and 0 at -100.
constexpr double kContrastGainPerStep{0.04};
constexpr double kContrastLossPerStep{0.01};

constexpr double kMidGray{128.0};

/// Brightness offset (in intensity levels) for one step of the slider.
constexpr double kBrightnessLevelsPerStep{2.55};

/// Returns the image in a format that the functions of this file can read and write.
QImage toSupportedFormat(const QImage& image)
{
    switch (image.format())
    {
        case QImage::Format_Grayscale8:
        case QImage::Format_RGB32:
        case QImage::Format_ARGB32:
            return image;

        case QImage::Format_Grayscale16:
            return image.convertToFormat(QImage::Format_Grayscale8);

        default:
            return image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32
                                                                 : QImage::Format_RGB32);
    }
}

} // namespace

bool isNeutral(const ViewConfig& view)
{
    return view.brightness == 0 && view.contrast == 0 && !view.autoContrast;
}

bool sameAdjustments(const ViewConfig& a, const ViewConfig& b)
{
    return a.brightness == b.brightness && a.contrast == b.contrast &&
           a.autoContrast == b.autoContrast;
}

IntensityRange measureRange(const QImage& image)
{
    if (image.isNull())
        return {};

    const QImage src = toSupportedFormat(image);
    const bool isGray = src.format() == QImage::Format_Grayscale8;

    const qint64 pixelCount = static_cast<qint64>(src.width()) * src.height();
    const int step = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(pixelCount) /
                                                            static_cast<double>(kSampleCount))));

    std::array<qint64, 256> histogram{};
    qint64 total = 0;

    for (int y = 0; y < src.height(); y += step)
    {
        const uchar* line = src.constScanLine(y);

        for (int x = 0; x < src.width(); x += step)
        {
            int value = 0;

            if (isGray)
                value = line[x];
            else
                value = qGray(reinterpret_cast<const QRgb*>(line)[x]);

            ++histogram[static_cast<size_t>(value)];
            ++total;
        }
    }

    const qint64 clipped = static_cast<qint64>(static_cast<double>(total) * kClippedFraction);

    int low = 0;
    qint64 cumulated = 0;
    while (low < 255 && cumulated + histogram[static_cast<size_t>(low)] <= clipped)
        cumulated += histogram[static_cast<size_t>(low++)];

    int high = 255;
    cumulated = 0;
    while (high > 0 && cumulated + histogram[static_cast<size_t>(high)] <= clipped)
        cumulated += histogram[static_cast<size_t>(high--)];

    if (high - low < kMinimumRangeWidth)
        return {};

    return {low, high};
}

std::array<uchar, 256> buildLut(int brightness, int contrast, IntensityRange range)
{
    const double width = static_cast<double>(std::max(1, range.high - range.low));
    const double stretch = 255.0 / width;

    const double gain = contrast >= 0 ? 1.0 + contrast * kContrastGainPerStep
                                      : 1.0 + contrast * kContrastLossPerStep;
    const double offset = brightness * kBrightnessLevelsPerStep;

    std::array<uchar, 256> lut{};

    for (int v = 0; v < 256; ++v)
    {
        const double stretched = std::clamp((v - range.low) * stretch, 0.0, 255.0);
        const double adjusted = (stretched - kMidGray) * gain + kMidGray + offset;

        lut[static_cast<size_t>(v)] =
            static_cast<uchar>(std::lround(std::clamp(adjusted, 0.0, 255.0)));
    }

    return lut;
}

QImage applyAdjustments(const QImage& image, const ViewConfig& view, IntensityRange range)
{
    if (image.isNull() || isNeutral(view))
        return image;

    if (!view.autoContrast)
        range = {};

    const std::array<uchar, 256> lut = buildLut(view.brightness, view.contrast, range);

    // Converted if needed, then detached: the original image is never touched.
    QImage out = toSupportedFormat(image);
    out.detach();

    const bool isGray = out.format() == QImage::Format_Grayscale8;

    for (int y = 0; y < out.height(); ++y)
    {
        uchar* line = out.scanLine(y);

        if (isGray)
        {
            for (int x = 0; x < out.width(); ++x)
                line[x] = lut[line[x]];
        }
        else
        {
            QRgb* pixels = reinterpret_cast<QRgb*>(line);

            for (int x = 0; x < out.width(); ++x)
            {
                const QRgb p = pixels[x];
                pixels[x] =
                    qRgba(lut[static_cast<size_t>(qRed(p))], lut[static_cast<size_t>(qGreen(p))],
                          lut[static_cast<size_t>(qBlue(p))], qAlpha(p));
            }
        }
    }

    return out;
}

QImage DisplayAdjuster::adjust(const QImage& image, const ViewConfig& view, bool smoothRange)
{
    if (isNeutral(view))
    {
        hasRange_ = false;
        return image;
    }

    IntensityRange range;

    if (view.autoContrast)
    {
        range = measureRange(image);

        if (smoothRange && hasRange_)
        {
            range.low = static_cast<int>(
                std::lround(range_.low + kRangeSmoothing * (range.low - range_.low)));
            range.high = static_cast<int>(
                std::lround(range_.high + kRangeSmoothing * (range.high - range_.high)));
        }

        range_ = range;
        hasRange_ = true;
    }
    else
    {
        hasRange_ = false;
    }

    return applyAdjustments(image, view, range);
}

} // namespace fluvel::display
