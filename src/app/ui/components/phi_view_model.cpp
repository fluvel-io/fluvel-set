// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "phi_view_model.hpp"
#include "phi_editor.hpp"


#ifdef FLUVEL_DEBUG
#include <QDebug>
#endif

namespace fluvel
{

// phi colors (Lout and Lin)
const QColor kOutColor{64, 0, 255};
const QColor kInColor{0, 230, 118};

// phi (Lout and Lin colors) in interactive mode
const QColor kInteractiveOuterColor{32, 0, 128};
const QColor kInteractiveInnerColor{0, 115, 59};

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
    updatePhiFromLists();

    connect(editor_, &PhiEditor::editedPhiChanged, this, &PhiViewModel::updateFromEditor);

    connect(editor_, &PhiEditor::editedPhiCleared, this, &PhiViewModel::onClearFromEditor);
}

void PhiViewModel::setConnectivity(fluvel_ip::Connectivity c)
{
    connectivity_ = c;

    updateLists();
    updatePhiFromLists();

    overlayVisible_ = false;
    emit overlayCleared();
    composeView();
}

void PhiViewModel::setBackground(const QImage& image)
{
    background_ = image.convertToFormat(QImage::Format_RGB32);

    updateLists();
    updatePhiFromLists();

    overlayVisible_ = false;
    emit overlayCleared();
    composeView();
}

void PhiViewModel::updateFromEditor()
{
#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << " updateFromEditor() " << __LINE__ << __func__;
#endif

    updateLists();
    updatePhiFromLists();

    overlayVisible_ = false;
    emit overlayCleared();
    composeView();
}

void PhiViewModel::onClearFromEditor()
{
    outerBoundary_.clear();
    innerBoundary_.clear();
    displayedPhi_ = background_;

    overlayVisible_ = false;
    emit overlayCleared();
    composeView();
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

void PhiViewModel::updatePhiFromLists()
{
#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << " updatePhiFromLists() " << __LINE__ << __func__;
#endif

    displayedPhi_ = background_;

    if (displayedPhi_.size() != listsGridSize_)
        return;

    Q_ASSERT(displayedPhi_.format() == QImage::Format_RGB32);

    const QRgb outColor = (interactiveMode_ ? kInteractiveOuterColor : kOutColor).rgb();
    const QRgb inColor = (interactiveMode_ ? kInteractiveInnerColor : kInColor).rgb();

    const int width = displayedPhi_.width();
    const int height = displayedPhi_.height();

    for (const auto& p : outerBoundary_)
    {
        if (p.y < 0 || p.y >= height || p.x < 0 || p.x >= width)
            continue;

        QRgb* line = reinterpret_cast<QRgb*>(displayedPhi_.scanLine(p.y));
        line[p.x] = outColor;
    }

    for (const auto& p : innerBoundary_)
    {
        if (p.y < 0 || p.y >= height || p.x < 0 || p.x >= width)
            continue;

        QRgb* line = reinterpret_cast<QRgb*>(displayedPhi_.scanLine(p.y));
        line[p.x] = inColor;
    }
}

void PhiViewModel::composeView()
{
#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << " composeView() " << __LINE__ << __func__;
#endif

    emit viewChanged(displayedPhi_);
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

void PhiViewModel::setInteractiveMode(bool enabled)
{
    interactiveMode_ = enabled;

    updatePhiFromLists(); // recalcul des couleurs atténuées
    composeView();
}

} // namespace fluvel
