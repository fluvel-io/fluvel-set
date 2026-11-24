// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QColor>
#include <QPointF>
#include <QVector>

namespace fluvel
{

/**
 * @brief A set of points drawn over the image.
 *
 * A layer is a generic annotation: the viewer only knows how to draw
 * colored points, not which algorithm produced them.
 */
struct PointLayer
{
    /// Points, in image coordinates.
    QVector<QPointF> points;

    /// Drawing color.
    QColor color{Qt::red};

    /// Hidden layers keep their points but are not drawn.
    bool visible{true};

    /// Scale from the layer coordinates to the displayed image coordinates.
    /// Set by the controller (e.g. downscale factor when the source image is shown).
    qreal scale{1.0};

    /// Draw the points as a smooth line at least a couple of screen pixels
    /// wide, so that a dense boundary stays continuous when the image is
    /// zoomed out (used for phi, which has one point per boundary pixel).
    bool smooth{false};
};

} // namespace fluvel
