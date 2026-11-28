// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "range_slider.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStylePainter>

#include <algorithm>
#include <cstdlib>

namespace fluvel
{

namespace
{

QStyleOptionSlider baseOption(const QWidget* widget, int minimum, int maximum)
{
    QStyleOptionSlider option;
    option.initFrom(widget);
    option.orientation = Qt::Horizontal;
    option.minimum = minimum;
    option.maximum = maximum;
    option.tickPosition = QSlider::NoTicks;
    option.upsideDown = false;
    return option;
}

} // namespace

RangeSlider::RangeSlider(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RangeSlider::setRange(int minimum, int maximum)
{
    minimum_ = minimum;
    maximum_ = std::max(minimum, maximum);

    setValues(low_, high_);
}

void RangeSlider::setValues(int low, int high)
{
    low = std::clamp(low, minimum_, maximum_);
    high = std::clamp(high, minimum_, maximum_);

    low_ = std::min(low, high);
    high_ = std::max(low, high);

    update();
}

QSize RangeSlider::sizeHint() const
{
    return {200, style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this) + 4};
}

QSize RangeSlider::minimumSizeHint() const
{
    return {60, sizeHint().height()};
}

QRect RangeSlider::grooveRect() const
{
    QStyleOptionSlider option = baseOption(this, minimum_, maximum_);
    option.rect = rect();

    return style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, this);
}

QRect RangeSlider::handleRect(int value) const
{
    QStyleOptionSlider option = baseOption(this, minimum_, maximum_);
    option.rect = rect();
    option.sliderValue = value;
    option.sliderPosition = value;

    return style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
}

int RangeSlider::positionFromValue(int value) const
{
    const QRect handle = handleRect(value);
    return handle.center().x();
}

int RangeSlider::valueFromPosition(int x) const
{
    // The handle center travels between the half widths of the handle at both ends.
    const QRect first = handleRect(minimum_);
    const QRect last = handleRect(maximum_);

    return QStyle::sliderValueFromPosition(minimum_, maximum_, x - first.center().x(),
                                           last.center().x() - first.center().x());
}

void RangeSlider::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);

    QStyleOptionSlider option = baseOption(this, minimum_, maximum_);
    option.rect = rect();
    option.sliderValue = low_;
    option.sliderPosition = low_;
    option.subControls = QStyle::SC_SliderGroove;

    painter.drawComplexControl(QStyle::CC_Slider, option);

    // Highlight of the selected part of the groove.
    const QRect groove = grooveRect();
    QRect selected = groove;
    selected.setLeft(positionFromValue(low_));
    selected.setRight(positionFromValue(high_));
    painter.fillRect(selected, palette().highlight());

    for (const int value : {low_, high_})
    {
        option.sliderValue = value;
        option.sliderPosition = value;
        option.subControls = QStyle::SC_SliderHandle;
        option.activeSubControls = QStyle::SC_None;

        const bool isActive = hasFocus() && ((value == low_ && active_ == Handle::Low) ||
                                             (value == high_ && active_ == Handle::High));

        option.state = isActive ? (option.state | QStyle::State_HasFocus)
                                : (option.state & ~QStyle::State_HasFocus);

        painter.drawComplexControl(QStyle::CC_Slider, option);
    }
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        event->ignore();
        return;
    }

    const QPoint pos = event->position().toPoint();
    const QRect lowRect = handleRect(low_);
    const QRect highRect = handleRect(high_);

    if (lowRect.contains(pos) && highRect.contains(pos))
    {
        // The handles overlap: the one that can still move in the direction of the click.
        pressed_ = pos.x() < lowRect.center().x() ? Handle::Low : Handle::High;
        grabOffset_ = pos.x() - positionFromValue(pressed_ == Handle::Low ? low_ : high_);
    }
    else if (lowRect.contains(pos))
    {
        pressed_ = Handle::Low;
        grabOffset_ = pos.x() - lowRect.center().x();
    }
    else if (highRect.contains(pos))
    {
        pressed_ = Handle::High;
        grabOffset_ = pos.x() - highRect.center().x();
    }
    else
    {
        // Click on the groove: the nearest handle jumps there.
        const int value = valueFromPosition(pos.x());
        pressed_ = std::abs(value - low_) <= std::abs(value - high_) ? Handle::Low : Handle::High;
        grabOffset_ = 0;
        moveHandle(pressed_, value);
    }

    active_ = pressed_;
    setFocus();
    update();
    event->accept();
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (pressed_ == Handle::None)
    {
        event->ignore();
        return;
    }

    moveHandle(pressed_, valueFromPosition(event->position().toPoint().x() - grabOffset_));
    event->accept();
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* event)
{
    pressed_ = Handle::None;
    event->accept();
}

void RangeSlider::keyPressEvent(QKeyEvent* event)
{
    const int current = active_ == Handle::Low ? low_ : high_;
    const int page = std::max((maximum_ - minimum_) / 10, 1);

    int target = current;

    switch (event->key())
    {
        case Qt::Key_Left:
        case Qt::Key_Down:
            target = current - 1;
            break;
        case Qt::Key_Right:
        case Qt::Key_Up:
            target = current + 1;
            break;
        case Qt::Key_PageDown:
            target = current - page;
            break;
        case Qt::Key_PageUp:
            target = current + page;
            break;
        case Qt::Key_Home:
            target = minimum_;
            break;
        case Qt::Key_End:
            target = maximum_;
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }

    moveHandle(active_, target);
}

void RangeSlider::moveHandle(Handle handle, int value)
{
    if (handle == Handle::Low)
    {
        const int next = std::clamp(value, minimum_, high_);

        if (next != low_)
        {
            low_ = next;
            update();
            emit lowValueChanged(low_);
        }
    }
    else if (handle == Handle::High)
    {
        const int next = std::clamp(value, low_, maximum_);

        if (next != high_)
        {
            high_ = next;
            update();
            emit highValueChanged(high_);
        }
    }
}

} // namespace fluvel
