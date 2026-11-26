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

} // namespace fluvel::display
