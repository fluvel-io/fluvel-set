// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "frame_rendering_utils.hpp"

#include <QPainter>

#include <algorithm>

namespace fluvel::frame_rendering_utils
{

void drawLayers(QImage& image, const QVector<PointLayer>& layers)
{
    if (image.isNull())
        return;

    const bool anyVisible = std::any_of(layers.begin(), layers.end(),
                                        [](const PointLayer& layer) { return layer.visible; });

    if (!anyVisible)
        return;

    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);

    painter.setBrush(Qt::NoBrush);

    QPen pen;
    pen.setWidthF(1.1);
    pen.setCosmetic(false);

    for (const PointLayer& layer : layers)
    {
        if (!layer.visible)
            continue;

        pen.setColor(layer.color);
        painter.setPen(pen);

        painter.save();
        painter.scale(layer.scale, layer.scale);
        painter.drawPoints(layer.points.data(), static_cast<int>(layer.points.size()));
        painter.restore();
    }
}

} // namespace fluvel::frame_rendering_utils