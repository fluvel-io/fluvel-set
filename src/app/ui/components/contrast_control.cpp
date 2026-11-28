// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "contrast_control.hpp"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

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
    lowSlider_ = new QSlider(Qt::Horizontal, this);
    highSlider_ = new QSlider(Qt::Horizontal, this);

    for (QSlider* slider : {lowSlider_, highSlider_})
        slider->setRange(0, kSliderSteps);

    lowSpin_ = new QDoubleSpinBox(this);
    highSpin_ = new QDoubleSpinBox(this);

    for (QDoubleSpinBox* spin : {lowSpin_, highSpin_})
        spin->setKeyboardTracking(false);

    resetButton_ = new QPushButton(tr("Reset"), this);
    resetButton_->setToolTip(
        tr("Values below the minimum are shown black, values above the maximum white."));

    auto* grid = new QGridLayout(this);
    grid->addWidget(new QLabel(tr("Minimum"), this), 0, 0);
    grid->addWidget(lowSpin_, 0, 1);
    grid->addWidget(lowSlider_, 1, 0, 1, 2);
    grid->addWidget(new QLabel(tr("Maximum"), this), 2, 0);
    grid->addWidget(highSpin_, 2, 1);
    grid->addWidget(highSlider_, 3, 0, 1, 2);
    grid->addWidget(resetButton_, 4, 0, 1, 2);
    grid->setRowStretch(5, 1);

    connect(lowSlider_, &QSlider::valueChanged, this,
            [this](int position)
            {
                moveEdge(Edge::Low, fromSliderPosition(position));
            });
    connect(highSlider_, &QSlider::valueChanged, this,
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
    const QSignalBlocker b1(lowSlider_);
    const QSignalBlocker b2(highSlider_);
    const QSignalBlocker b3(lowSpin_);
    const QSignalBlocker b4(highSpin_);

    lowSlider_->setValue(toSliderPosition(current_.low));
    highSlider_->setValue(toSliderPosition(current_.high));
    lowSpin_->setValue(current_.low);
    highSpin_->setValue(current_.high);
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
