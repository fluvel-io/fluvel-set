// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "image_preview_worker.hpp"

#include "elapsed_timer.hpp"
#include "image_adapters.hpp"
#include "image_pipeline.hpp"

#include <QMetaObject>
#include <QMutexLocker>

#include <cassert>

namespace fluvel
{

ImagePreviewWorker::ImagePreviewWorker()
    : QObject(nullptr)
{
    moveToThread(&thread_);
    thread_.start();
}

ImagePreviewWorker::~ImagePreviewWorker()
{
    thread_.quit();
    thread_.wait();
}

void ImagePreviewWorker::submit(quint64 id, const QImage& input, const DownscaleParams& downscale,
                                const fluvel_ip::ProcessingParams& processing)
{
    {
        const QMutexLocker lock(&mutex_);
        pending_ = Request{id, input, downscale, processing};
    }

    QMetaObject::invokeMethod(this, [this] { run(); }, Qt::QueuedConnection);
}

void ImagePreviewWorker::run()
{
    std::optional<Request> request;

    {
        const QMutexLocker lock(&mutex_);
        request.swap(pending_);
    }

    // Already handled by a previous call: only the latest request is run.
    if (!request)
        return;

    QImage downscaled = request->input;

    if (downscaled.isNull())
    {
        emit previewReady(request->id, QImage(), QImage(), 0.0);
        return;
    }

    if (downscaled.format() != QImage::Format_Grayscale8 &&
        downscaled.format() != QImage::Format_RGB32)
    {
        emit previewReady(request->id, downscaled, downscaled, 0.0);
        return;
    }

    if (request->downscale.downscaleEnabled)
    {
        const int df = request->downscale.downscaleFactor;

        assert(df == 2 || df == 4 || df == 8 || df == 16);

        downscaled = downscaled.scaled(downscaled.width() / df, downscaled.height() / df,
                                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // Independent copy: the pipeline works on its own buffer.
    auto input = imageOwnerFromQImage(downscaled);

    fluvel_ip::ImagePipeline filter;
    filter.reset(input.view());

    fluvel_ip::ElapsedTimer measurementTimer;
    measurementTimer.start();

    filter.apply(input.view(), request->processing);

    const double elapsedSec = measurementTimer.elapsedSec();

    QImage processed = toQImageCopy(filter.outputView());

    if (processed.isNull())
        processed = downscaled;

    emit previewReady(request->id, downscaled, processed, elapsedSec);
}

} // namespace fluvel
