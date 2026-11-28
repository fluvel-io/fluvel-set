// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_slice_view.hpp"

#include "image_adapters.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
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

void SliceCanvas::setOverlayText(const QString& text)
{
    overlayText_ = text;
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

    if (!overlayText_.isEmpty())
    {
        painter.setPen(QColor(255, 235, 59));
        painter.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignTop, overlayText_);
    }
}

void SliceCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::RightButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    dragging_ = true;
    lastDragPos_ = event->position().toPoint();
    event->accept();
}

void SliceCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!dragging_)
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint pos = event->position().toPoint();
    const QPoint delta = pos - lastDragPos_;
    lastDragPos_ = pos;

    if (!delta.isNull())
        emit dragged(delta.x(), delta.y());

    event->accept();
}

void SliceCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton)
        dragging_ = false;

    QWidget::mouseReleaseEvent(event);
}

VolumeSliceView::VolumeSliceView(fluvel_ip::VolumeAxis axis, QWidget* parent)
    : QWidget(parent)
    , axis_(axis)
{
    canvas_ = new SliceCanvas(this);
    canvas_->setToolTip(tr("Right-click and drag to adjust the contrast."));
    connect(canvas_, &SliceCanvas::dragged, this, &VolumeSliceView::windowDragged);

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

QString VolumeSliceView::windowText() const
{
    // The usual way to describe the range: width (W) and level, the center (L).
    const double width = window_.high - window_.low;
    const double level = (window_.high + window_.low) / 2.0;

    return QStringLiteral("W: %1  L: %2").arg(width, 0, 'g', 6).arg(level, 0, 'g', 6);
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
        canvas_->setOverlayText({});
        indexLabel_->clear();
        return;
    }

    const int index = slider_->value();

    canvas_->setImage(toQImageCopy(volume_->sliceToGray8(axis_, index, window_)));
    canvas_->setOverlayText(windowText());

    indexLabel_->setText(
        QStringLiteral("%1 / %2").arg(slider_->value() + 1).arg(slider_->maximum() + 1));
}

} // namespace fluvel
