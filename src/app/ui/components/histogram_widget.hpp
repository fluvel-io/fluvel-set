// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "histogram.hpp"

#include <QRectF>
#include <QSize>
#include <QVector>
#include <QWidget>

#include <array>

class QEvent;
class QMouseEvent;
class QPaintEvent;

namespace fluvel
{

/**
 * @brief Widget drawing the intensity histogram of an image.
 *
 * A grayscale image gives one plot. A color image gives three plots stacked vertically
 * (red, green, blue) sharing the same horizontal axis, each one with its own vertical scale
 * so every channel stays readable.
 *
 * The colors follow the application palette, so the plots fit a light or a dark theme.
 * The two extreme levels (0 and 255) are often much more populated than the others
 * (clipped shadows or highlights): they would flatten the whole curve, so the vertical scale
 * of a channel is computed from the levels 1 to 254 and the extreme bars are clipped.
 *
 * When the mouse is over the plots, the level under the cursor is shown with the share of
 * pixels and the pixel count of each channel.
 */
class HistogramWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget* parent = nullptr);

    /// Sets the histogram to draw (an empty histogram, without samples, shows a message).
    void setHistogram(const fluvel_ip::Histogram& histogram);

    /// Switches between a linear and a logarithmic vertical scale.
    void setLogScale(bool enabled);

    bool logScale() const noexcept
    {
        return logScale_;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    /// Area containing all the plots (the horizontal axis labels are below it).
    QRectF plotArea() const;

    /// Area of the plot of a channel (the whole plot area for a grayscale image).
    QRectF channelRect(int channel) const;

    /// Height of a bar in [0, 1] for a count of a channel.
    double normalized(int channel, uint64_t count) const;

    /// Largest count of the levels 1..254 of a channel (of all the levels if the others are empty).
    uint64_t referenceMaximum(int channel) const;

    fluvel_ip::Histogram histogram_;
    std::array<uint64_t, 3> referenceMax_{};
    bool logScale_{false};

    /// Level under the cursor, -1 if none.
    int hoverLevel_{-1};
};

} // namespace fluvel
