// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "image_controller.hpp"
#include "contour_adapters.hpp"
#include "contour_layers.hpp"
#include "file_utils.hpp"

#include <QFileInfo>
#include <QImageReader>

#include <cmath>

#ifdef FLUVEL_DEBUG
#include "image_debug.hpp"
#endif

namespace fluvel
{

namespace
{

/// True for the QImage formats storing more than 8 bits per channel.
bool hasMoreThan8BitsPerChannel(QImage::Format format)
{
    switch (format)
    {
        case QImage::Format_Grayscale16:
        case QImage::Format_RGBX64:
        case QImage::Format_RGBA64:
        case QImage::Format_RGBA64_Premultiplied:
        case QImage::Format_BGR30:
        case QImage::Format_A2BGR30_Premultiplied:
        case QImage::Format_RGB30:
        case QImage::Format_A2RGB30_Premultiplied:
            return true;

        default:
            return false;
    }
}

} // namespace

ImageController::ImageController(const ImageSessionSettings& session, QObject* parent)
    : QObject(parent)
{
    onImageSettingsChanged(session);
    onImageDisplaySettingsChanged(session.display);

    connect(&processingWorker_, &ImageProcessingWorker::processedImageReady, this,
            &ImageController::onProcessedImageReady);

    connect(&processingWorker_, &ImageProcessingWorker::contourUpdated, this,
            &ImageController::onContourUpdated, Qt::QueuedConnection);

    connect(&processingWorker_, &ImageProcessingWorker::stateChanged, this,
            &ImageController::onStateChanged);

    connect(&processingWorker_, &ImageProcessingWorker::diagnosticsUpdated, this,
            &ImageController::onDiagnosticsUpdated);
}

void ImageController::loadImage(const QString& path)
{
    if (path.isEmpty())
        return;

    if (const QString problem = file_utils::readableFileError(path); !problem.isEmpty())
    {
        emit errorOccurred(problem);
        return;
    }

    if (!file_utils::isSupportedImage(path))
    {
        emit errorOccurred(tr("Unsupported image format."));
        return;
    }

    QImageReader reader(path);

    const QString extension = file_utils::normalizeImageFormat(QFileInfo(path).suffix());
    const QString detected = file_utils::normalizeImageFormat(QString::fromLatin1(reader.format()));

    QImage img = reader.read();

    if (!extension.isEmpty() && extension != detected)
    {
        const QString warningStr = tr("Image file extension differs from detected format: .%1 → %2")
                                       .arg(extension, detected);

        emit warningOccurred(warningStr);
    }

    if (img.isNull())
    {
        emit errorOccurred(tr("Failed to load image: %1").arg(reader.errorString()));
        return;
    }

    // The processing works on 8 bits per channel: tell the user when precision is lost.
    if (hasMoreThan8BitsPerChannel(img.format()))
        emit warningOccurred(tr("The image has more than 8 bits per channel: it is converted "
                                "to 8 bits."));

    inputImage_ = img;

    if (inputImage_.isGrayscale())
        inputImage_ = inputImage_.convertToFormat(QImage::Format_Grayscale8);
    else
        inputImage_ = inputImage_.convertToFormat(QImage::Format_RGB32);

    // The processed image of the previous one no longer applies. Until the new
    // one comes back from the worker, the image without filters is displayed.
    processedImage_ = QImage();

    reinitializeWorker();
    refreshView();

    emit inputImageReady(inputImage_);
    emit imageOpened(path);
}

void ImageController::downscaleImage()
{
    if (inputImage_.isNull())
        return;

    if (inputImage_.format() != QImage::Format_Grayscale8 &&
        inputImage_.format() != QImage::Format_RGB32)
        return;

    if (computeConfig_.downscale.downscaleEnabled)
    {
        const int df = computeConfig_.downscale.downscaleFactor;

        assert(df == 2 || df == 4);

        downscaledImage_ = inputImage_.scaled(inputImage_.width() / df, inputImage_.height() / df,
                                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        downscaledImage_ = inputImage_;
    }
}

bool ImageController::reinitializeWorker()
{
    if (inputImage_.isNull())
        return false;

    downscaleImage();

    if (downscaledImage_.isNull())
        return false;

    if (computeConfig_.initialPhi.isNull())
        return false;

    ImageComputeConfig config = computeConfig_;

#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << ":" << __LINE__ << __func__
             << "phi:" << image_debug::describeImage(config.initialPhi);
#endif

    if (computeConfig_.initialPhi.size() != downscaledImage_.size())
    {
        config.initialPhi = computeConfig_.initialPhi.scaled(
            downscaledImage_.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    // No layers clear the displayed contours.
    lastOuterContour_.clear();
    lastInnerContour_.clear();
    hasContour_ = false;

    emit contourUpdated({});

#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << ":" << __LINE__ << __func__
             << "phi:" << image_debug::describeImage(config.initialPhi);
#endif

    initGeneration_ = processingWorker_.initialize(downscaledImage_, config);

    return true;
}

void ImageController::onProcessedImageReady(const QImage& processed, quint64 generation)
{
    // The image of an older initialization, overtaken by a new request.
    if (generation != initGeneration_)
        return;

    processedImage_ = processed;

    refreshView();
}

void ImageController::onImageSettingsChanged(const ImageSessionSettings& session)
{
    computeConfig_ = session.compute;

    reinitializeWorker();
}

void ImageController::onImageDisplaySettingsChanged(const DisplayConfig& display)
{
    bool needsRefresh = (displayConfig_.background != display.background);

    displayConfig_ = display;

    if (needsRefresh)
        refreshView();

    // Contour colors and visibility may have changed: restyle the displayed contour.
    if (hasContour_)
    {
        emit contourUpdated(currentLayers());
    }
}

void ImageController::refreshView()
{
    if (inputImage_.isNull())
        return;

    // While the worker is processing a new image, the same image without
    // filters (downscaled as the processed one will be) is shown instead.
    const bool processed = !processedImage_.isNull();

    QImage img;

    if (displayConfig_.background == BackgroundImage::Source)
        img = inputImage_;
    else if (displayConfig_.background == BackgroundImage::Preprocessed)
        img = processed ? processedImage_ : downscaledImage_;

    if (!img.isNull())
        emit displayedImageReady(img);
}

void ImageController::onContourUpdated(const fluvel_ip::Contour& outerContour,
                                       const fluvel_ip::Contour& innerContour)
{
    lastOuterContour_ = convertToQVector(outerContour);
    lastInnerContour_ = convertToQVector(innerContour);
    hasContour_ = true;

    emit contourUpdated(currentLayers());

    // Let the worker publish the next frame.
    processingWorker_.acknowledgeFrame();
}

QVector<PointLayer> ImageController::currentLayers() const
{
    return contour_layers::make(lastOuterContour_, lastInnerContour_, displayConfig_,
                                contour_layers::scaleFor(displayConfig_, computeConfig_.downscale));
}

void ImageController::restart()
{
    processingWorker_.restart();
}

void ImageController::togglePause()
{
    processingWorker_.togglePause();
}

void ImageController::step()
{
    processingWorker_.step();
}

void ImageController::converge()
{
    processingWorker_.converge();
}

void ImageController::onStateChanged(fluvel::WorkerState state)
{
    emit stateChanged(state);
}

void ImageController::onDiagnosticsUpdated(const fluvel_ip::ContourDiagnostics& diag,
                                           bool streaming)
{
    auto formatChannels = [](const fluvel_ip::ChannelVector& v)
    {
        const auto& values = v.values();

        if (values.empty())
            return QString(tr("n/a"));

        if (values.size() == 1)
            return QString::number(values[0]);

        QString s("(");
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            s += QString::number(values[i]);
            if (i + 1 < values.size())
                s += ", ";
        }
        s += ")";
        return s;
    };

    QString s;

    s += QString(tr("Step: %1\n")).arg(diag.stepCount);
    s += QString(tr("State: %1\n")).arg(fluvel_ip::toString(diag.state));

    if (diag.stoppingStatus != fluvel_ip::StoppingStatus::None)
        s += QString(tr("Reason: %1\n")).arg(fluvel_ip::toString(diag.stoppingStatus));
    else
        s += QString(tr("Reason: \n"));

    if (!diag.meanInside.values().empty())
    {
        s += QString(tr("Mean in: %1\n")).arg(formatChannels(diag.meanInside));

        s += QString(tr("Mean out: %1\n")).arg(formatChannels(diag.meanOutside));
    }

    // The distance is a variation between two successive steps: it is NaN until
    // the first measure, so the line is skipped instead of showing "nan".
    if (!std::isnan(diag.hausdorffDistance))
        s += QString(tr("Hausdorff: %1 %\n")).arg(diag.hausdorffDistance, 0, 'f', 2);

    // Time spent computing iterations (waits and pauses excluded).
    s += QString(tr("Compute time: %1 s\n")).arg(diag.elapsedSec, 0, 'f', 2);

    // A new run restarts the counters from zero.
    if (diag.stepCount < lastStepCount_ || diag.elapsedSec < lastComputeSec_)
    {
        lastStepCount_ = 0;
        lastComputeSec_ = 0.0;
        window_.clear();
        smoothedTimer_.invalidate();
    }

    const int newSteps = diag.stepCount - lastStepCount_;
    const double newSec = diag.elapsedSec - lastComputeSec_;

    lastStepCount_ = diag.stepCount;
    lastComputeSec_ = diag.elapsedSec;

    if (streaming)
    {
        // Speed and contour size change at every frame: average them over a
        // sliding window and refresh them only a few times per second, so
        // that the numbers can be read.
        window_.push_back({diag.contourSize, newSteps, newSec});

        if (window_.size() > kSmoothingWindow)
            window_.pop_front();

        if (!smoothedTimer_.isValid() || smoothedTimer_.elapsed() >= kSmoothedRefreshMs)
        {
            double sumContour = 0.0;
            long long sumSteps = 0;
            double sumSec = 0.0;

            for (const auto& sample : window_)
            {
                sumContour += double(sample.contourSize);
                sumSteps += sample.steps;
                sumSec += sample.sec;
            }

            const double speed = sumSec > 0.0 ? double(sumSteps) / sumSec : 0.0;

            smoothedSpeed_ = QString::number(speed, 'f', 0);
            smoothedContour_ = QString::number(sumContour / double(window_.size()), 'f', 0);
            smoothedTimer_.restart();
        }

        s += QString(tr("Speed: %1 it/s\n")).arg(smoothedSpeed_);
        s += QString(tr("Contour: %1 pts")).arg(smoothedContour_);
    }
    else
    {
        // Pause, single step, initial state or end of run: exact values.
        window_.clear();
        smoothedTimer_.invalidate();

        // The speed has no meaning for a pause or a single step, but it is the
        // average speed of the run once it is over.
        const bool runOver = diag.stoppingStatus != fluvel_ip::StoppingStatus::None;

        if (runOver && diag.elapsedSec > 0.0)
            s += QString(tr("Speed: %1 it/s\n")).arg(diag.stepCount / diag.elapsedSec, 0, 'f', 0);
        else
            s += QString(tr("Speed: %1 it/s\n")).arg(QStringLiteral("--"));

        s += QString(tr("Contour: %1 pts")).arg(diag.contourSize);
    }

    emit textDiagnosticsUpdated(s);
}

} // namespace fluvel
