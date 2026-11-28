// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_slice_view.hpp"

#include "image_adapters.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>

#include <utility>

namespace fluvel
{

SliceCanvas::SliceCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(120, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SliceCanvas::setImage(const QImage& image)
{
    image_ = image;
    update();
}

QSize SliceCanvas::sizeHint() const
{
    return {320, 320};
}

void SliceCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (image_.isNull())
        return;

    const QSize target = image_.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect area(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2),
                     target);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(area, image_);
}

VolumeSliceView::VolumeSliceView(fluvel_ip::VolumeAxis axis, QWidget* parent)
    : QWidget(parent)
    , axis_(axis)
{
    canvas_ = new SliceCanvas(this);

    slider_ = new QSlider(Qt::Horizontal, this);
    slider_->setEnabled(false);

    indexLabel_ = new QLabel(this);
    indexLabel_->setMinimumWidth(70);
    indexLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* sliderRow = new QHBoxLayout;
    sliderRow->addWidget(slider_, 1);
    sliderRow->addWidget(indexLabel_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(canvas_, 1);
    layout->addLayout(sliderRow);

    connect(slider_, &QSlider::valueChanged, this,
            [this](int index)
            {
                refresh();
                emit sliceChanged(index);
            });
}

void VolumeSliceView::setVolume(std::shared_ptr<const AnyVolume> volume)
{
    volume_ = std::move(volume);

    const int count = volume_ ? volume_->sliceCount(axis_) : 0;
    window_ = volume_ ? volume_->defaultWindow() : fluvel_ip::VoxelWindow{};

    // Block the signals so the display is refreshed only once, after the new range is set.
    slider_->blockSignals(true);
    slider_->setRange(0, count > 0 ? count - 1 : 0);
    slider_->setValue(count / 2);
    slider_->setEnabled(count > 1);
    slider_->blockSignals(false);

    refresh();
    emit sliceChanged(slider_->value());
}

void VolumeSliceView::setWindow(const fluvel_ip::VoxelWindow& window)
{
    window_ = window;
    refresh();
}

int VolumeSliceView::sliceIndex() const
{
    return slider_->value();
}

void VolumeSliceView::refresh()
{
    if (!volume_ || volume_->isEmpty())
    {
        canvas_->setImage({});
        indexLabel_->clear();
        return;
    }

    const int index = slider_->value();

    canvas_->setImage(toQImageCopy(volume_->sliceToGray8(axis_, index, window_)));

    indexLabel_->setText(
        QStringLiteral("%1 / %2").arg(slider_->value() + 1).arg(slider_->maximum() + 1));
}

} // namespace fluvel
