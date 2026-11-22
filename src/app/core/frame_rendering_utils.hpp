// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "point_layer.hpp"

#include <QColor>
#include <QImage>
#include <QVector>

namespace fluvel
{

namespace frame_rendering_utils
{

/**
 * @brief Draws styled layers of points onto an image.
 *
 * The rendering parameters match those used by the interactive image viewer.
 * Hidden layers are skipped.
 *
 * @param image Image onto which the layers are rendered.
 * @param layers Layers of points. Each layer carries its own style
 *        (color, visibility) and its scale to the image coordinates.
 */
void drawLayers(QImage& image, const QVector<PointLayer>& layers);

} // namespace frame_rendering_utils

} // namespace fluvel