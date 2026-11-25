// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "image_settings_controller.hpp"
#include <QCoreApplication>

#ifdef FLUVEL_DEBUG
#include <QDebug>
#endif

namespace fluvel
{

ImageSettingsController::ImageSettingsController(const ImageComputeConfig& config, QObject* parent)
    : QObject(parent)
{
    editedDownscaleParams_ = config.downscale;
    editedProcessingParams_ = config.processing;

    phiEditor_ = std::make_unique<PhiEditor>(config.initialPhi);

    phiViewModel_ =
        std::make_unique<PhiViewModel>(phiEditor_.get(), config.contourConfig.connectivity);

    connect(phiViewModel_.get(), &PhiViewModel::viewChanged, this,
            &ImageSettingsController::onViewChanged);

    connect(phiViewModel_.get(), &PhiViewModel::layersChanged, this,
            &ImageSettingsController::layersChanged);
    connect(phiViewModel_.get(), &PhiViewModel::overlayChanged, this,
            &ImageSettingsController::overlayChanged);
    connect(phiViewModel_.get(), &PhiViewModel::overlayCleared, this,
            &ImageSettingsController::overlayCleared);

    connect(&previewWorker_, &ImagePreviewWorker::previewReady, this,
            &ImageSettingsController::onPreviewReady);
}

void ImageSettingsController::updateEditedConfig(
    const DownscaleParams& downscaleParams, const fluvel_ip::ProcessingParams& processingParams)
{
    editedDownscaleParams_ = downscaleParams;
    editedProcessingParams_ = processingParams;

    refreshPreview();
}

ShapeInfo ImageSettingsController::computeShapeInfo(const UiShapeInfo& uiShape,
                                                    const QSize& targetSize)
{
    ShapeInfo info;

    info.type = uiShape.shape;

    const float canvasWidth = static_cast<float>(targetSize.width());
    const float canvasHeight = static_cast<float>(targetSize.height());

    float centerXPercent = static_cast<float>(uiShape.x) / 100.f;
    float centerYPercent = static_cast<float>(uiShape.y) / 100.f;

    float centerX = (centerXPercent + 0.5f) * canvasWidth;
    float centerY = (centerYPercent + 0.5f) * canvasHeight;

    float width = static_cast<float>(uiShape.width) / 100.f * canvasWidth;
    float height = static_cast<float>(uiShape.height) / 100.f * canvasHeight;

    float topLeftX = centerX - width / 2.f;
    float topLeftY = centerY - height / 2.f;

    info.boundingBox = QRect(static_cast<int>(topLeftX), static_cast<int>(topLeftY),
                             static_cast<int>(width), static_cast<int>(height));

    return info;
}

void ImageSettingsController::addShape(UiShapeInfo uiShape)
{
    if (!initializationEnabled_)
        return;

    if (!phiEditor_)
        return;

    if (phiEditor_->phi().isNull())
        return;

    auto shape = computeShapeInfo(uiShape, phiEditor_->phi().size());

    phiEditor_->addShape(shape);
}

void ImageSettingsController::subtractShape(UiShapeInfo uiShape)
{
    if (!initializationEnabled_)
        return;

    if (!phiEditor_)
        return;

    if (phiEditor_->phi().isNull())
        return;

    auto shape = computeShapeInfo(uiShape, phiEditor_->phi().size());

    phiEditor_->subtractShape(shape);
}

void ImageSettingsController::clearPhi()
{
    if (phiEditor_)
        phiEditor_->clear();
}

void ImageSettingsController::onInputImageReady(const QImage& input)
{
    input_ = input;

    // The preview shown (if any) is the one of the previous image.
    awaitingFirstPreview_ = true;

    if (!viewVisible_)
    {
        needsRefresh_ = true;
        return;
    }

    phiEditor_->setSize(input_.size());

    refreshPreview();
}

void ImageSettingsController::onViewChanged(const QImage& imageSettings)
{
    if (!imageSettings.isNull())
        emit viewChanged(imageSettings);
}

void ImageSettingsController::onConnectivityChanged(fluvel_ip::Connectivity c)
{
    if (!phiViewModel_)
        return;

    phiViewModel_->setConnectivity(c);
}

void ImageSettingsController::onUpdateOverlay(UiShapeInfo uiShape)
{
    if (!initializationEnabled_ || !phiViewModel_)
        return;

    if (phiViewModel_->phi().isNull())
        return;

    auto shape = computeShapeInfo(uiShape, phiViewModel_->phi().size());

    phiViewModel_->setOverlay(shape);
}

QImage ImageSettingsController::commit()
{
    QImage initialPhi;

    if (phiEditor_)
        initialPhi = phiEditor_->commit();

    return initialPhi;
}

void ImageSettingsController::revert()
{
    if (phiEditor_)
        phiEditor_->revert();
}

void ImageSettingsController::setInteractiveMode(bool enabled)
{
    if (!phiViewModel_)
        return;

    initializationEnabled_ = enabled;

    phiViewModel_->setInteractiveMode(enabled);

    if (enabled)
        phiViewModel_->showOverlay();
    else
        phiViewModel_->hideOverlay();
}

void ImageSettingsController::setContourColors(const QColor& outer, const QColor& inner)
{
    if (phiViewModel_)
        phiViewModel_->setContourColors(outer, inner);
}

void ImageSettingsController::refreshPreview()
{
    // Any result of an older request becomes outdated.
    ++lastRequestId_;

    if (input_.isNull())
    {
        downscaled_ = input_;
        processed_ = input_;
        return;
    }

    emit processingStarted();

    // New image: until its preview is ready, show it without filters (and
    // without phi, which refers to the size of the preview) rather than the
    // previous image. This is decided from the state, not from timing: once
    // the preview has arrived, nothing shows the unprocessed image again.
    if (awaitingFirstPreview_)
    {
        emit layersChanged({});
        emit overlayCleared();
        emit viewChanged(input_);
    }

    previewWorker_.submit(lastRequestId_, input_, editedDownscaleParams_, editedProcessingParams_);
}

void ImageSettingsController::onPreviewReady(quint64 id, const QImage& downscaled,
                                             const QImage& processed, double elapsedSec)
{
    // A newer request has been made since: its own result will follow.
    if (id != lastRequestId_)
        return;

    downscaled_ = downscaled;
    processed_ = processed;

    awaitingFirstPreview_ = false;

    emit filterPipelineProcessed(elapsedSec);

    if (!processed_.isNull())
        phiViewModel_->setBackground(processed_);
}

void ImageSettingsController::setViewVisible(bool v)
{
    viewVisible_ = v;

    if (viewVisible_ && needsRefresh_)
    {
        needsRefresh_ = false;
        refreshPreview();
    }
}

} // namespace fluvel
