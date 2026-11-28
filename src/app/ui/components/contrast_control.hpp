// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_display.hpp"

#include <QWidget>

class QDoubleSpinBox;
class QPushButton;
class QSlider;

namespace fluvel
{

/**
 * @brief Controls choosing the range of voxel values shown on screen.
 *
 * Two sliders (with spin boxes) set the minimum, displayed black, and the maximum, displayed
 * white. The minimum always stays below the maximum. A button restores the default range.
 * Typical use is a 16-bit or CT volume, where the interesting values only span a small part
 * of the whole range.
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

    QSlider* lowSlider_{nullptr};
    QSlider* highSlider_{nullptr};
    QDoubleSpinBox* lowSpin_{nullptr};
    QDoubleSpinBox* highSpin_{nullptr};
    QPushButton* resetButton_{nullptr};
};

} // namespace fluvel
