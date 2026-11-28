// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "any_volume.hpp"

#include <QImage>
#include <QWidget>

#include <memory>

class QLabel;
class QMouseEvent;
class QPaintEvent;
class QSlider;

namespace fluvel
{

/**
 * @brief Paints one image, scaled to fit while keeping its aspect ratio.
 */
class SliceCanvas final : public QWidget
{
    Q_OBJECT

public:
    explicit SliceCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& image);

    /// Text drawn in the top left corner (nothing when empty).
    void setOverlayText(const QString& text);

    QSize sizeHint() const override;

signals:
    /// Emitted while the right button is dragged, with the movement in pixels.
    void dragged(int dx, int dy);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QImage image_;
    QString overlayText_;
    QPoint lastDragPos_;
    bool dragging_{false};
};

/**
 * @brief View of one slice of a volume, with a slider to move through the slices.
 */
class VolumeSliceView final : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeSliceView(fluvel_ip::VolumeAxis axis, QWidget* parent = nullptr);

    /// Gives the view the volume to display (shared, not copied). The slider goes to the middle.
    void setVolume(std::shared_ptr<const AnyVolume> volume);

    int sliceIndex() const;

    /// Sets the range of voxel values shown from black to white.
    void setWindow(const fluvel_ip::VoxelWindow& window);

signals:
    /// Emitted when the displayed slice changes.
    void sliceChanged(int index);

    /// Emitted while the right button is dragged on the image, with the movement in pixels.
    void windowDragged(int dx, int dy);

private:
    void refresh();

    /// "W: width  L: level" of the displayed range.
    QString windowText() const;

    fluvel_ip::VolumeAxis axis_;
    fluvel_ip::VoxelWindow window_;
    std::shared_ptr<const AnyVolume> volume_;

    SliceCanvas* canvas_{nullptr};
    QSlider* slider_{nullptr};
    QLabel* indexLabel_{nullptr};
};

} // namespace fluvel
