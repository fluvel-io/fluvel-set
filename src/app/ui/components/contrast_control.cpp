// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "contrast_control.hpp"

#include "range_slider.hpp"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>

namespace fluvel
{

namespace
{

/// Number of positions of the sliders.
constexpr int kSliderSteps = 1000;

} // namespace

ContrastControl::ContrastControl(QWidget* parent)
    : QWidget(parent)
{
    slider_ = new RangeSlider(this);
    slider_->setRange(0, kSliderSteps);

    lowSpin_ = new QDoubleSpinBox(this);
    highSpin_ = new QDoubleSpinBox(this);

    for (QDoubleSpinBox* spin : {lowSpin_, highSpin_})
        spin->setKeyboardTracking(false);

    resetButton_ = new QPushButton(tr("Reset"), this);
    resetButton_->setToolTip(
        tr("Values below the minimum are shown black, values above the maximum white."));

    auto* grid = new QGridLayout(this);
    grid->addWidget(slider_, 0, 0, 1, 2);
    grid->addWidget(new QLabel(tr("Minimum"), this), 1, 0);
    grid->addWidget(lowSpin_, 1, 1);
    grid->addWidget(new QLabel(tr("Maximum"), this), 2, 0);
    grid->addWidget(highSpin_, 2, 1);
    grid->addWidget(resetButton_, 3, 0, 1, 2);
    grid->setRowStretch(4, 1);

    connect(slider_, &RangeSlider::lowValueChanged, this,
            [this](int position)
            {
                moveEdge(Edge::Low, fromSliderPosition(position));
            });
    connect(slider_, &RangeSlider::highValueChanged, this,
            [this](int position)
            {
                moveEdge(Edge::High, fromSliderPosition(position));
            });
    connect(lowSpin_, &QDoubleSpinBox::valueChanged, this,
            [this](double value)
            {
                moveEdge(Edge::Low, value);
            });
    connect(highSpin_, &QDoubleSpinBox::valueChanged, this,
            [this](double value)
            {
                moveEdge(Edge::High, value);
            });

    connect(resetButton_, &QPushButton::clicked, this,
            [this]
            {
                current_ = default_;
                showWindow();
                emit windowChanged(current_);
            });

    setEnabled(false);
}

void ContrastControl::setLimits(double lowest, double highest,
                                const fluvel_ip::VoxelWindow& defaultWindow, bool integerValues)
{
    // The sliders must reach the default range too (0..255 for an 8-bit volume whose values
    // only span a part of it).
    lowest = std::min(lowest, defaultWindow.low);
    highest = std::max(highest, defaultWindow.high);

    lowest_ = lowest;
    highest_ = highest;
    integerValues_ = integerValues;
    default_ = defaultWindow;
    current_ = defaultWindow;

    const double span = highest - lowest;

    // Smallest distance kept between the two edges.
    minGap_ = integerValues ? 1.0 : std::max(span / kSliderSteps, 1e-12);

    int decimals = 0;

    if (!integerValues && span > 0.0)
        decimals = std::clamp(3 - static_cast<int>(std::floor(std::log10(span))), 0, 6);

    const double step = integerValues ? 1.0 : std::max(span / 100.0, 1e-12);

    for (QDoubleSpinBox* spin : {lowSpin_, highSpin_})
    {
        const QSignalBlocker blocker(spin);
        spin->setDecimals(decimals);
        spin->setRange(lowest, highest);
        spin->setSingleStep(step);
    }

    setEnabled(span >= minGap_);
    showWindow();
}

fluvel_ip::VoxelWindow ContrastControl::window() const
{
    return current_;
}

void ContrastControl::moveEdge(Edge edge, double value)
{
    fluvel_ip::VoxelWindow next = current_;

    if (edge == Edge::Low)
        next.low = std::clamp(value, lowest_, std::max(lowest_, next.high - minGap_));
    else
        next.high = std::clamp(value, std::min(highest_, next.low + minGap_), highest_);

    if (integerValues_)
    {
        next.low = std::round(next.low);
        next.high = std::round(next.high);
    }

    const bool changed = next.low != current_.low || next.high != current_.high;

    current_ = next;

    // The widgets are always updated: a value refused by the clamp goes back to the edge.
    showWindow();

    if (changed)
        emit windowChanged(current_);
}

void ContrastControl::showWindow()
{
    const QSignalBlocker b1(lowSpin_);
    const QSignalBlocker b2(highSpin_);

    slider_->setValues(toSliderPosition(current_.low), toSliderPosition(current_.high));
    lowSpin_->setValue(current_.low);
    highSpin_->setValue(current_.high);
}

void ContrastControl::dragWindow(int dx, int dy)
{
    if (!isEnabled())
        return;

    const double span = highest_ - lowest_;
    const double unit = span / kDragPixelsForFullRange;

    double width = (current_.high - current_.low) - dx * unit;
    double level = (current_.high + current_.low) / 2.0 + dy * unit;

    width = std::clamp(width, minGap_, span);
    level = std::clamp(level, lowest_, highest_);

    fluvel_ip::VoxelWindow next{level - width / 2.0, level + width / 2.0};

    // Keep the range inside the limits by shifting it, so its width is preserved.
    if (next.low < lowest_)
    {
        next.high += lowest_ - next.low;
        next.low = lowest_;
    }

    if (next.high > highest_)
    {
        next.low -= next.high - highest_;
        next.high = highest_;
    }

    if (integerValues_)
    {
        next.low = std::round(next.low);
        next.high = std::round(next.high);
    }

    if (next.low == current_.low && next.high == current_.high)
        return;

    current_ = next;
    showWindow();
    emit windowChanged(current_);
}

int ContrastControl::toSliderPosition(double value) const
{
    const double span = highest_ - lowest_;

    if (span <= 0.0)
        return 0;

    return static_cast<int>(std::lround((value - lowest_) / span * kSliderSteps));
}

double ContrastControl::fromSliderPosition(int position) const
{
    return lowest_ + (highest_ - lowest_) * position / kSliderSteps;
}

} // namespace fluvel
