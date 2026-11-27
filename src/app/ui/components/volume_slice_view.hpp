// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_owner.hpp"

#include <QImage>
#include <QWidget>

#include <memory>

class QLabel;
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

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage image_;
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
    void setVolume(std::shared_ptr<const fluvel_ip::VolumeOwner> volume);

    int sliceIndex() const;

signals:
    /// Emitted when the displayed slice changes.
    void sliceChanged(int index);

private:
    void refresh();

    fluvel_ip::VolumeAxis axis_;
    std::shared_ptr<const fluvel_ip::VolumeOwner> volume_;

    SliceCanvas* canvas_{nullptr};
    QSlider* slider_{nullptr};
    QLabel* indexLabel_{nullptr};
};

} // namespace fluvel
