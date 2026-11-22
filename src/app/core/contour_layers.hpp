// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "point_layer.hpp"

#include <QPointF>
#include <QVector>

namespace fluvel
{
struct DisplayConfig;
struct DownscaleParams;

/**
 * @brief Mapping between the active contour and generic point layers.
 *
 * The image viewer only knows styled layers of points. This is the single
 * place where layer 0 is the outer contour and layer 1 the inner contour,
 * and where the display configuration is turned into layer styles.
 */
namespace contour_layers
{

/// Index of the outer contour layer.
inline constexpr int kOuter = 0;

/// Index of the inner contour layer.
inline constexpr int kInner = 1;

/**
 * @brief Computes the scale from contour coordinates to the displayed image.
 *
 * Contours are expressed in processing image coordinates. When the source
 * image is displayed while downscaled processing is enabled, they must be
 * scaled back to the source image space. This is the single place of that rule.
 *
 * @param displayConfig Display configuration (background image).
 * @param downscaleParams Processing downscale parameters.
 *
 * @return Layer scale.
 */
[[nodiscard]] qreal scaleFor(const DisplayConfig& displayConfig,
                             const DownscaleParams& downscaleParams);

/**
 * @brief Builds the styled layers of an active contour.
 *
 * @param outer Outer contour points.
 * @param inner Inner contour points.
 * @param displayConfig Display configuration (contour colors and visibility).
 * @param scale Layer scale, see scaleFor().
 *
 * @return Layers, indexed by kOuter and kInner.
 */
[[nodiscard]] QVector<PointLayer> make(const QVector<QPointF>& outer, const QVector<QPointF>& inner,
                                       const DisplayConfig& displayConfig, qreal scale);

/**
 * @brief Applies a new display configuration to existing contour layers.
 *
 * Points are kept, only the styles and the scale change.
 *
 * @param layers Layers built by make().
 * @param displayConfig Display configuration (contour colors and visibility).
 * @param scale Layer scale, see scaleFor().
 *
 * @return Restyled layers.
 */
[[nodiscard]] QVector<PointLayer> restyle(const QVector<PointLayer>& layers,
                                          const DisplayConfig& displayConfig, qreal scale);

} // namespace contour_layers

} // namespace fluvel
