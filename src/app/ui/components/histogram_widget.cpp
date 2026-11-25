// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "histogram_widget.hpp"

#include <QColor>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fluvel
{

namespace
{

constexpr int kMarginLeft{8};
constexpr int kMarginRight{8};
constexpr int kMarginTop{8};
constexpr int kMarginBottom{22};

/// Vertical gap between the plots of two channels.
constexpr int kPlotGap{6};

constexpr int kLevels{fluvel_ip::kHistogramLevels};

/// Colors of the plots, derived from the application palette (light or dark theme).
struct PlotColors
{
    bool dark{false};
    QColor background;
    QColor border;
    QColor grid;
    QColor hover;
    QColor text;
    QColor readoutBackground;
};

PlotColors plotColors(const QPalette& palette)
{
    PlotColors colors;

    colors.background = palette.color(QPalette::Base);
    colors.dark = colors.background.lightness() < 128;

    colors.text = palette.color(QPalette::Text);
    colors.border = palette.color(QPalette::Mid);

    colors.grid = colors.text;
    colors.grid.setAlpha(30);

    colors.hover = colors.text;
    colors.hover.setAlpha(140);

    colors.readoutBackground = palette.color(QPalette::Window);
    colors.readoutBackground.setAlpha(225);

    return colors;
}

/// Color of a curve: red, green, blue for a color image, gray otherwise.
/// The shades are chosen to stay readable on a dark or on a light background.
QColor curveColor(int channel, bool color, bool dark)
{
    if (!color)
        return dark ? QColor(200, 200, 200) : QColor(90, 90, 90);

    switch (channel)
    {
        case 0:
            return dark ? QColor(230, 60, 60) : QColor(205, 45, 45);

        case 1:
            return dark ? QColor(60, 200, 80) : QColor(35, 155, 65);

        default:
            return dark ? QColor(80, 130, 255) : QColor(50, 95, 225);
    }
}

} // namespace

HistogramWidget::HistogramWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(minimumSizeHint());
}

QSize HistogramWidget::sizeHint() const
{
    return {440, 340};
}

QSize HistogramWidget::minimumSizeHint() const
{
    return {260, 220};
}

void HistogramWidget::setHistogram(const fluvel_ip::Histogram& histogram)
{
    histogram_ = histogram;

    for (int c = 0; c < 3; ++c)
        referenceMax_[c] = (c < histogram_.channelCount()) ? referenceMaximum(c) : 0;

    update();
}

void HistogramWidget::setLogScale(bool enabled)
{
    if (logScale_ == enabled)
        return;

    logScale_ = enabled;
    update();
}

QRectF HistogramWidget::plotArea() const
{
    return QRectF(kMarginLeft, kMarginTop, width() - kMarginLeft - kMarginRight,
                  height() - kMarginTop - kMarginBottom);
}

QRectF HistogramWidget::channelRect(int channel) const
{
    const QRectF area = plotArea();
    const int count = histogram_.channelCount();

    if (count == 1)
        return area;

    const double height = (area.height() - kPlotGap * (count - 1)) / count;

    return QRectF(area.left(), area.top() + channel * (height + kPlotGap), area.width(), height);
}

uint64_t HistogramWidget::referenceMaximum(int channel) const
{
    uint64_t inner = 0;
    uint64_t all = 0;

    for (int level = 0; level < kLevels; ++level)
    {
        const uint64_t count = histogram_.bins[channel][level];

        all = std::max(all, count);

        if (level != 0 && level != kLevels - 1)
            inner = std::max(inner, count);
    }

    return inner > 0 ? inner : all;
}

double HistogramWidget::normalized(int channel, uint64_t count) const
{
    const uint64_t reference = referenceMax_[channel];

    if (reference == 0)
        return 0.0;

    const double value = logScale_ ? std::log1p(static_cast<double>(count)) /
                                         std::log1p(static_cast<double>(reference))
                                   : static_cast<double>(count) / static_cast<double>(reference);

    // The extreme levels may exceed the reference: they are clipped.
    return std::min(value, 1.0);
}

void HistogramWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const PlotColors colors = plotColors(palette());

    const QFontMetrics metrics(font());
    const QRectF area = plotArea();
    const bool hasData = histogram_.samples > 0;

    const int channelCount = hasData ? histogram_.channelCount() : 1;

    // --- Backgrounds and grids
    for (int c = 0; c < channelCount; ++c)
    {
        const QRectF plot = channelRect(c);

        painter.setPen(QPen(colors.border, 1));
        painter.setBrush(colors.background);
        painter.drawRoundedRect(plot, 4, 4);

        painter.setPen(QPen(colors.grid, 1));

        for (int i = 1; i < 4; ++i)
        {
            const double x = plot.left() + plot.width() * i / 4.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }

        for (int i = 1; i < 4; ++i)
        {
            const double y = plot.top() + plot.height() * i / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
    }

    // --- Horizontal axis labels (shared by all the plots)
    for (int i = 0; i <= 4; ++i)
    {
        const double x = area.left() + area.width() * i / 4.0;
        const int level = (i == 4) ? kLevels - 1 : i * (kLevels / 4);

        const QString label = QString::number(level);
        const double textWidth = metrics.horizontalAdvance(label);

        // Keep the first and last labels inside the widget.
        double textX = x - textWidth / 2.0;
        textX = std::clamp(textX, 0.0, static_cast<double>(width()) - textWidth);

        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(QPointF(textX, area.bottom() + metrics.ascent() + 4), label);
    }

    // --- No data
    if (!hasData)
    {
        painter.setPen(colors.text);
        painter.drawText(area, Qt::AlignCenter, tr("No image"));
        return;
    }

    // --- Curves
    static const char* const kNames[3] = {"R", "G", "B"};

    for (int c = 0; c < channelCount; ++c)
    {
        const QRectF plot = channelRect(c);
        const double binWidth = plot.width() / kLevels;

        painter.save();

        QPainterPath clip;
        clip.addRoundedRect(plot, 4, 4);
        painter.setClipPath(clip);

        QPainterPath path;
        path.moveTo(plot.left(), plot.bottom());

        for (int level = 0; level < kLevels; ++level)
        {
            const double x = plot.left() + (level + 0.5) * binWidth;
            const double y =
                plot.bottom() - normalized(c, histogram_.bins[c][level]) * plot.height();

            path.lineTo(x, y);
        }

        path.lineTo(plot.right(), plot.bottom());
        path.closeSubpath();

        QColor fill = curveColor(c, histogram_.color, colors.dark);
        fill.setAlpha(190);

        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(path);

        painter.restore();

        // Channel name, in the corner of its plot.
        if (histogram_.color)
        {
            painter.setPen(colors.dark ? curveColor(c, true, true).lighter(150)
                                       : curveColor(c, true, false).darker(115));
            painter.drawText(QPointF(plot.left() + 6, plot.top() + metrics.ascent() + 3),
                             QString::fromLatin1(kNames[c]));
        }
    }

    // --- Level under the cursor
    if (hoverLevel_ >= 0)
    {
        const double binWidth = area.width() / kLevels;
        const double x = area.left() + (hoverLevel_ + 0.5) * binWidth;

        painter.setPen(QPen(colors.hover, 1));

        for (int c = 0; c < channelCount; ++c)
        {
            const QRectF plot = channelRect(c);
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }

        QStringList lines;
        lines << tr("Level %1").arg(hoverLevel_);

        for (int c = 0; c < histogram_.channelCount(); ++c)
        {
            const uint64_t count = histogram_.bins[c][hoverLevel_];
            const double percent = 100.0 * static_cast<double>(count) / histogram_.samples;

            const QString name = histogram_.color ? QString::fromLatin1(kNames[c]) : tr("Gray");

            lines << QStringLiteral("%1: %2 %  (%3)")
                         .arg(name)
                         .arg(percent, 0, 'f', 2)
                         .arg(static_cast<qulonglong>(count));
        }

        int textWidth = 0;
        for (const QString& line : std::as_const(lines))
            textWidth = std::max(textWidth, metrics.horizontalAdvance(line));

        const double boxWidth = textWidth + 12;
        const double boxHeight = metrics.lineSpacing() * lines.size() + 8;

        // The box goes on the side opposite to the cursor, at the top of the plots.
        const double boxX = (x > area.center().x()) ? area.left() + 6 : area.right() - boxWidth - 6;
        const QRectF box(boxX, area.top() + 6, boxWidth, boxHeight);

        painter.setPen(QPen(colors.border, 1));
        painter.setBrush(colors.readoutBackground);
        painter.drawRoundedRect(box, 4, 4);

        painter.setPen(colors.text);

        for (int i = 0; i < lines.size(); ++i)
        {
            painter.drawText(QPointF(box.left() + 6,
                                     box.top() + 4 + metrics.ascent() + i * metrics.lineSpacing()),
                             lines[i]);
        }
    }
}

void HistogramWidget::mouseMoveEvent(QMouseEvent* event)
{
    const QRectF area = plotArea();
    const double x = event->position().x();

    int level = -1;

    if (histogram_.samples > 0 && x >= area.left() && x <= area.right() && area.width() > 0.0)
    {
        level = static_cast<int>((x - area.left()) / area.width() * kLevels);
        level = std::clamp(level, 0, kLevels - 1);
    }

    if (level != hoverLevel_)
    {
        hoverLevel_ = level;
        update();
    }
}

void HistogramWidget::leaveEvent(QEvent*)
{
    if (hoverLevel_ != -1)
    {
        hoverLevel_ = -1;
        update();
    }
}

} // namespace fluvel
