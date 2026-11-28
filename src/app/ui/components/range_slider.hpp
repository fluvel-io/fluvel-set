// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QWidget>

namespace fluvel
{

/**
 * @brief Horizontal slider with two handles, choosing a range of values.
 *
 * The part of the groove between the handles is highlighted. The handles can be dragged, a
 * click on the groove moves the nearest handle, and the arrow keys, Page Up/Down, Home and End
 * move the handle that was used last. The low handle never goes above the high one.
 *
 * The signals are only emitted when the user changes a value, not by setRange() / setValues().
 */
class RangeSlider final : public QWidget
{
    Q_OBJECT

public:
    explicit RangeSlider(QWidget* parent = nullptr);

    /// Sets the smallest and largest values. The handles are moved inside the range.
    void setRange(int minimum, int maximum);

    /// Moves the handles (clamped to the range, low <= high) without emitting any signal.
    void setValues(int low, int high);

    int lowValue() const
    {
        return low_;
    }

    int highValue() const
    {
        return high_;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void lowValueChanged(int value);
    void highValueChanged(int value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Handle
    {
        None,
        Low,
        High
    };

    /// Rectangle of the handle placed at @p value.
    QRect handleRect(int value) const;

    /// Rectangle of the groove.
    QRect grooveRect() const;

    int valueFromPosition(int x) const;
    int positionFromValue(int value) const;

    /// Moves a handle from the user, and emits the signal if the value changed.
    void moveHandle(Handle handle, int value);

    int minimum_{0};
    int maximum_{100};
    int low_{0};
    int high_{100};

    Handle pressed_{Handle::None};
    Handle active_{Handle::High};
    int grabOffset_{0};
};

} // namespace fluvel
