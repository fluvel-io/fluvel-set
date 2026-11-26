// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

/**
 * @file color_palette.hpp
 * @brief Predefined colors of the application: the single place where they are defined.
 *
 * The colors are saturated enough to stand out on natural images and, for most of them,
 * bright enough for dark ones. Purple and green are the colors of the Fluvel logo.
 *
 * Purple and blue are deliberately darker than the others: next to the bright green, they
 * keep two adjacent contours easy to tell apart, and they stay visible on light images.
 *
 * To change a color, edit it here only: the color selector, the default contour colors and the
 * outline of the shape being placed all read their values from this file.
 *
 * @note The color names shown in the color selector are translated separately,
 *       in ColorSelectorWidget.
 */

#pragma once

#include "color.hpp"

namespace fluvel::palette
{

inline constexpr fluvel_ip::Rgb_uc kRed{255u, 59u, 48u};
inline constexpr fluvel_ip::Rgb_uc kOrange{255u, 149u, 0u};
inline constexpr fluvel_ip::Rgb_uc kYellow{255u, 204u, 0u};
inline constexpr fluvel_ip::Rgb_uc kGreen{52u, 199u, 89u};
inline constexpr fluvel_ip::Rgb_uc kCyan{50u, 200u, 255u};
inline constexpr fluvel_ip::Rgb_uc kBlue{0u, 85u, 210u};
inline constexpr fluvel_ip::Rgb_uc kPurple{96u, 64u, 255u};
inline constexpr fluvel_ip::Rgb_uc kPink{255u, 45u, 85u};

/// Default color of the outer contour.
inline constexpr fluvel_ip::Rgb_uc kDefaultOuterContour{kPurple};

/// Default color of the inner contour.
inline constexpr fluvel_ip::Rgb_uc kDefaultInnerContour{kGreen};

} // namespace fluvel::palette
