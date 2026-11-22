// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "contour_layers.hpp"
#include "application_settings_types.hpp"
#include "color_adapters.hpp"

namespace fluvel::contour_layers
{

qreal scaleFor(const DisplayConfig& displayConfig, const DownscaleParams& downscaleParams)
{
    if (downscaleParams.downscaleEnabled && displayConfig.background == BackgroundImage::Source)
        return static_cast<qreal>(downscaleParams.downscaleFactor);

    return 1.0;
}

QVector<PointLayer> make(const QVector<QPointF>& outer, const QVector<QPointF>& inner,
                         const DisplayConfig& displayConfig, qreal scale)
{
    QVector<PointLayer> layers(2);

    layers[kOuter] = {outer, toQColor(displayConfig.contour.outerColor),
                      displayConfig.contour.outerVisible, scale};

    layers[kInner] = {inner, toQColor(displayConfig.contour.innerColor),
                      displayConfig.contour.innerVisible, scale};

    return layers;
}

QVector<PointLayer> restyle(const QVector<PointLayer>& layers, const DisplayConfig& displayConfig,
                            qreal scale)
{
    const QVector<QPointF> outer = layers.size() > kOuter ? layers[kOuter].points : QVector<QPointF>{};
    const QVector<QPointF> inner = layers.size() > kInner ? layers[kInner].points : QVector<QPointF>{};

    return make(outer, inner, displayConfig, scale);
}

} // namespace fluvel::contour_layers
