// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

/**
 * @file display_adjustment.hpp
 * @brief Brightness, contrast and auto-contrast applied to the image shown in the viewer.
 *
 * These adjustments only change what is drawn on screen. The image given to the algorithms,
 * the pixel values read under the cursor and the exported files are never modified.
 */

#pragma once

#include "application_settings_types.hpp"

#include <QImage>

#include <array>

namespace fluvel::display
{

/**
 * @brief Range of intensities (0-255) that auto-contrast stretches to the full scale.
 */
struct IntensityRange
{
    int low{0};
    int high{255};
};

/**
 * @brief Returns true when the view options leave the image unchanged.
 */
bool isNeutral(const ViewConfig& view);

/**
 * @brief Returns true when the two view options give the same brightness, contrast and
 * auto-contrast (the other view options are ignored).
 */
bool sameAdjustments(const ViewConfig& a, const ViewConfig& b);

/**
 * @brief Measures the intensity range of an image for auto-contrast.
 *
 * The lowest and highest 1% of the pixels are ignored, so that a few isolated dark or bright
 * pixels do not prevent the stretch. Only a sample of the pixels is read, which keeps the cost
 * low on large images. For color images the luminance is used.
 *
 * @param image Image to measure.
 * @return The measured range, or the full scale if the image is empty or almost flat.
 */
IntensityRange measureRange(const QImage& image);

/**
 * @brief Builds the table that maps each input intensity to its displayed intensity.
 *
 * The range is first stretched to the full scale (identity for 0-255), then the contrast
 * is applied around mid-gray, and finally the brightness offset.
 *
 * @param brightness Brightness, from -100 to 100 (0 = unchanged).
 * @param contrast Contrast, from -100 to 100 (0 = unchanged).
 * @param range Range to stretch to the full scale.
 */
std::array<uchar, 256> buildLut(int brightness, int contrast, IntensityRange range);

/**
 * @brief Returns the image to display, with the adjustments of the view options applied.
 *
 * The same table is used for the three color channels, so colors are not shifted. The alpha
 * channel is left unchanged. If the options leave the image unchanged, it is returned as is.
 * Images in other formats than 8-bit grayscale or RGB are converted first.
 *
 * @param image Original image.
 * @param view View options holding brightness, contrast and the auto-contrast flag.
 * @param range Range used when auto-contrast is enabled (ignored otherwise).
 */
QImage applyAdjustments(const QImage& image, const ViewConfig& view, IntensityRange range);

/**
 * @brief Prepares the images shown by a viewer, from the original ones.
 *
 * It owns what must be remembered from one image to the next: on video, the range measured by
 * auto-contrast is smoothed over the frames, which avoids flicker. The viewer only has to give
 * its images and its view options.
 */
class DisplayAdjuster
{
public:
    /**
     * @brief Returns the image to show for an original image.
     *
     * @param image Original image (never modified).
     * @param view View options holding brightness, contrast and the auto-contrast flag.
     * @param smoothRange True for a new frame of a stream: the auto-contrast range is then
     * mixed with the ones of the previous frames. False when the same image is shown again
     * (for example after a change of option): the range is measured directly.
     * @return The adjusted image, or the original one if the options leave it unchanged.
     */
    QImage adjust(const QImage& image, const ViewConfig& view, bool smoothRange);

private:
    /// Range used for the previous image, valid if hasRange_ is true.
    IntensityRange range_{};
    bool hasRange_{false};
};

} // namespace fluvel::display
