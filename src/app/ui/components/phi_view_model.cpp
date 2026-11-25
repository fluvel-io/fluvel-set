// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "phi_view_model.hpp"
#include "contour_adapters.hpp"
#include "phi_editor.hpp"


#ifdef FLUVEL_DEBUG
#include <QDebug>
#endif

namespace fluvel
{

// Dimming factor (QColor::darker) applied to the contour colors in interactive mode.
constexpr int kInteractiveDarkerFactor{200};

PhiViewModel::PhiViewModel(PhiEditor* editor, fluvel_ip::Connectivity connectivity, QObject* parent)
    : QObject(parent)
    , editor_(editor)
    , connectivity_(connectivity)
{
    Q_ASSERT(editor_);

    background_ = QImage(editor_->phi().width(), editor_->phi().height(), QImage::Format_RGB32);

    background_.fill(Qt::black);

    phi_ = editor_->phi();

    updateLists();

    connect(editor_, &PhiEditor::editedPhiChanged, this, &PhiViewModel::updateFromEditor);

    connect(editor_, &PhiEditor::editedPhiCleared, this, &PhiViewModel::onClearFromEditor);
}

void PhiViewModel::setConnectivity(fluvel_ip::Connectivity c)
{
    connectivity_ = c;

    updateLists();
    emitLayers();

    overlayVisible_ = false;
    emit overlayCleared();
}

void PhiViewModel::setBackground(const QImage& image)
{
    background_ = image.convertToFormat(QImage::Format_RGB32);

    updateLists();

    // The image first: it sets the size of the scene the layers refer to.
    emit viewChanged(background_);
    emitLayers();

    overlayVisible_ = false;
    emit overlayCleared();
}

void PhiViewModel::updateFromEditor()
{
#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << " updateFromEditor() " << __LINE__ << __func__;
#endif

    updateLists();
    emitLayers();

    overlayVisible_ = false;
    emit overlayCleared();
}

void PhiViewModel::onClearFromEditor()
{
    outerBoundary_.clear();
    innerBoundary_.clear();
    emitLayers();

    overlayVisible_ = false;
    emit overlayCleared();
}

void PhiViewModel::updateLists()
{
    if (!editor_)
        return;

    phi_ = editor_->phi();

    if (phi_.size() != background_.size())
    {
        phi_ = phi_.scaled(background_.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    const int w = phi_.width();
    const int h = phi_.height();

    listsGridSize_ = phi_.size();
    outerBoundary_.clear();
    innerBoundary_.clear();

    const bool eight = connectivity_ != fluvel_ip::Connectivity::Four;

    // A pixel belongs to a boundary list when one of its neighbors has another
    // value. Pixels outside the image are neutral. The rows above and below are
    // read through pointers, which is much faster than a bounds-checked access
    // per neighbor on large images.
    for (int y = 0; y < h; ++y)
    {
        const uchar* cur = phi_.constScanLine(y);
        const uchar* up = (y > 0) ? phi_.constScanLine(y - 1) : nullptr;
        const uchar* down = (y + 1 < h) ? phi_.constScanLine(y + 1) : nullptr;

        for (int x = 0; x < w; ++x)
        {
            const uchar value = cur[x];

            if (value != 0 && value != 255)
                continue;

            const bool hasLeft = x > 0;
            const bool hasRight = x + 1 < w;

            bool redundant = (!hasLeft || cur[x - 1] == value) && (!hasRight || cur[x + 1] == value) &&
                             (!up || up[x] == value) && (!down || down[x] == value);

            if (redundant && eight)
            {
                redundant = (!up || ((!hasLeft || up[x - 1] == value) &&
                                     (!hasRight || up[x + 1] == value))) &&
                            (!down || ((!hasLeft || down[x - 1] == value) &&
                                       (!hasRight || down[x + 1] == value)));
            }

            if (redundant)
                continue;

            if (value == 0)
                outerBoundary_.emplace_back(x, y);
            else
                innerBoundary_.emplace_back(x, y);
        }
    }
}

void PhiViewModel::emitLayers()
{
    const QColor outColor =
        interactiveMode_ ? outerColor_.darker(kInteractiveDarkerFactor) : outerColor_;
    const QColor inColor =
        interactiveMode_ ? innerColor_.darker(kInteractiveDarkerFactor) : innerColor_;

    QVector<PointLayer> layers(2);

    layers[0].points = convertToQVector(outerBoundary_);
    layers[0].color = outColor;
    layers[0].smooth = true;

    layers[1].points = convertToQVector(innerBoundary_);
    layers[1].color = inColor;
    layers[1].smooth = true;

    emit layersChanged(layers);
}

void PhiViewModel::setOverlay(const ShapeInfo& overlayShape)
{
    overlayShape_ = overlayShape;
    overlayVisible_ = true;
    emitOverlay();
}

void PhiViewModel::showOverlay()
{
    overlayVisible_ = true;
    emitOverlay();
}

void PhiViewModel::hideOverlay()
{
    overlayVisible_ = false;
    emit overlayCleared();
}

void PhiViewModel::emitOverlay()
{
    if (!overlayVisible_ || !overlayShape_.boundingBox.isValid())
    {
        emit overlayCleared();
        return;
    }

    // The shape is a vector item of the viewer: it stays sharp at any zoom and
    // moving it never touches the image.
    const QRectF box(overlayShape_.boundingBox.normalized());

    QPainterPath path;

    if (overlayShape_.type == ShapeType::Rectangle)
        path.addRect(box);
    else
        path.addEllipse(box);

    emit overlayChanged(path);
}

void PhiViewModel::setContourColors(const QColor& outer, const QColor& inner)
{
    outerColor_ = outer;
    innerColor_ = inner;

    emitLayers();
}

void PhiViewModel::setInteractiveMode(bool enabled)
{
    interactiveMode_ = enabled;

    emitLayers(); // colors are dimmed in interactive mode
}

} // namespace fluvel
