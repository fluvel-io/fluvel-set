// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_display.hpp"

#include <QWidget>

class QDoubleSpinBox;
class QPushButton;

namespace fluvel
{

class RangeSlider;

/**
 * @brief Controls choosing the range of voxel values shown on screen.
 *
 * A slider with two handles (and two spin boxes) sets the minimum, displayed black, and the
 * maximum, displayed white. The minimum always stays below the maximum. A button restores the
 * default range. Typical use is a 16-bit or CT volume, where the interesting values only span a
 * small part of the whole range.
 */
class ContrastControl final : public QWidget
{
    Q_OBJECT

public:
    explicit ContrastControl(QWidget* parent = nullptr);

    /**
     * @brief Sets the values the sliders can reach and the range shown by default.
     *
     * The current range becomes @p defaultWindow. The control is disabled when
     * @p lowest >= @p highest (nothing to adjust).
     *
     * @param lowest Smallest value of the volume (widened to include @p defaultWindow).
     * @param highest Largest value of the volume (widened to include @p defaultWindow).
     * @param defaultWindow Range restored by the reset button.
     * @param integerValues True to show whole numbers (integer voxels).
     */
    void setLimits(double lowest, double highest, const fluvel_ip::VoxelWindow& defaultWindow,
                   bool integerValues);

    /// Range currently chosen.
    fluvel_ip::VoxelWindow window() const;

    /**
     * @brief Changes the range from a mouse drag on an image.
     *
     * Dragging to the right narrows the range (more contrast), to the left widens it. Dragging
     * up moves the range toward the low values (brighter image), down toward the high values.
     * A drag of kDragPixelsForFullRange pixels crosses the whole range of values.
     *
     * @param dx Horizontal movement in pixels (positive to the right).
     * @param dy Vertical movement in pixels (positive downward).
     */
    void dragWindow(int dx, int dy);

    /// Pixels of mouse drag that correspond to the whole range of values.
    static constexpr int kDragPixelsForFullRange = 400;

signals:
    /// Emitted when the user changes the range.
    void windowChanged(const fluvel_ip::VoxelWindow& window);

private:
    enum class Edge
    {
        Low,
        High
    };

    /// Moves one edge, keeping the minimum below the maximum, then updates the widgets.
    void moveEdge(Edge edge, double value);

    /// Shows the current range in the sliders and spin boxes without sending any signal.
    void showWindow();

    int toSliderPosition(double value) const;
    double fromSliderPosition(int position) const;

    double lowest_{0.0};
    double highest_{0.0};
    double minGap_{1.0};
    bool integerValues_{true};
    fluvel_ip::VoxelWindow default_;
    fluvel_ip::VoxelWindow current_;

    RangeSlider* slider_{nullptr};
    QDoubleSpinBox* lowSpin_{nullptr};
    QDoubleSpinBox* highSpin_{nullptr};
    QPushButton* resetButton_{nullptr};
};

} // namespace fluvel
