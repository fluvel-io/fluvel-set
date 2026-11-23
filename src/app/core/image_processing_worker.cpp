// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "image_processing_worker.hpp"

#include "region_gray_speed_model.hpp"
#include "region_color_speed_model.hpp"
#include "speed_model.hpp"

#include "image_adapters.hpp"
#include "image_pipeline.hpp"

#include "elapsed_timer.hpp"

#include <QMetaObject>
#include <QMutexLocker>

#ifdef FLUVEL_DEBUG
#include <QDebug>
#endif

#include <cassert>
#include <utility>

namespace fluvel
{

ImageProcessingWorker::ImageProcessingWorker()
    : QObject(nullptr)
{
    // Needed to queue these types across threads.
    qRegisterMetaType<fluvel_ip::Contour>("fluvel_ip::Contour");
    qRegisterMetaType<fluvel_ip::ContourDiagnostics>("fluvel_ip::ContourDiagnostics");
    qRegisterMetaType<fluvel::WorkerState>("fluvel::WorkerState");

    moveToThread(&thread_);
    thread_.start();
}

ImageProcessingWorker::~ImageProcessingWorker()
{
    // Pending events are discarded when the event loop exits.
    thread_.quit();
    thread_.wait();
}

template <typename F>
void ImageProcessingWorker::post(F&& f)
{
    QMetaObject::invokeMethod(this, std::forward<F>(f), Qt::QueuedConnection);
}

void ImageProcessingWorker::acknowledgeFrame() noexcept
{
    frameInFlight_.store(false, std::memory_order_release);
}

void ImageProcessingWorker::restart()
{
    post([this] { restartImpl(); });
}

void ImageProcessingWorker::togglePause()
{
    post([this] { togglePauseImpl(); });
}

void ImageProcessingWorker::stepImpl()
{
    post([this] { stepImpl(); });
}

void ImageProcessingWorker::convergeImpl()
{
    post([this] { convergeImpl(); });
}

void ImageProcessingWorker::initialize(const QImage& image, const ImageComputeConfig& config)
{
    {
        const QMutexLocker lock(&requestMutex_);
        pendingInit_ = InitRequest{image, config};
    }

    post([this] { initializeImpl(); });
}

void ImageProcessingWorker::restartImpl()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    setMode(RunMode::Interactive);

    const auto& fc = config_.processing;

    const bool hasRandom = fc.gaussianNoiseEnabled || fc.saltNoiseEnabled || fc.speckleNoiseEnabled;

    if (processingDirty_ || hasRandom)
        applyProcessing();

    initializeActiveContour();

    if (state_ != WorkerState::Ready)
        return;

    publish(true);

    initialShown_ = true;

    start();
}

void ImageProcessingWorker::start()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    setState(WorkerState::Running);
    scheduleTick();
}

void ImageProcessingWorker::togglePauseImpl()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    setMode(RunMode::Interactive);

    if (state_ == WorkerState::Running)
    {
        suspend();
    }
    else if (state_ == WorkerState::Suspended)
    {
        resume();
    }
    else if (state_ == WorkerState::Ready)
    {
        if (!initialShown_)
        {
            publish(true);

            initialShown_ = true;
        }
        else
        {
            resume();
        }
    }
}

void ImageProcessingWorker::step()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    setMode(RunMode::Interactive);

    if (state_ == WorkerState::Running)
    {
        suspend();
    }
    else if (state_ == WorkerState::Suspended)
    {
        performStep();
    }
    else if (state_ == WorkerState::Ready)
    {
        if (!initialShown_)
        {
            publish(true);

            initialShown_ = true;
        }
        else
        {
            performStep();
            setState(WorkerState::Suspended);
        }
    }
}

void ImageProcessingWorker::performStep()
{
    if (!stepOnceAlgo())
    {
        finalizeAndPrepareNextRun();
    }
    else
    {
        publish(true);
    }
}

void ImageProcessingWorker::converge()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    if (state_ == WorkerState::Ready)
    {
        publish(true);

        initialShown_ = true;
    }

    setMode(RunMode::Converge);

    if (state_ != WorkerState::Running)
        start();
}

bool ImageProcessingWorker::stepOnceAlgo()
{
    assert(activeContour_ != nullptr);

    if (activeContour_->isStopped())
    {
        initializeActiveContour();
    }
    else
    {
        if (activeContour_->isFirstIteration())
        {
            isMeasuring_ = true;
            measurementTimer_.start();
        }

        activeContour_->step();
    }

    return !activeContour_->isStopped();
}

void ImageProcessingWorker::applyProcessing()
{
    setState(WorkerState::Initializing);

    processedImage_ = image_;

    if (image_.isNull())
    {
        processingDirty_ = false;
        return;
    }

#ifdef FLUVEL_DEBUG
    qDebug() << __FILE__ << " applyProcessing() " << __LINE__ << __func__;
#endif

    fluvel_ip::ImagePipeline preprocess;
    auto img = imageOwnerFromQImage(image_);

    preprocess.reset(img.view());
    preprocess.apply(img.view(), config_.processing);

    processedImage_ = toQImageCopy(preprocess.outputView());

    //--------------------------------------------------
    // 5. Emit
    //--------------------------------------------------
    if (!processedImage_.isNull())
        emit processedImageReady(processedImage_);

    processingDirty_ = false;
}

void ImageProcessingWorker::initializeImpl()
{
    std::optional<InitRequest> request;

    {
        const QMutexLocker lock(&requestMutex_);
        request.swap(pendingInit_);
    }

    // Already handled by a previous call: only the latest request is run.
    if (!request)
        return;

    setState(WorkerState::Initializing);

    image_ = std::move(request->image);
    config_ = std::move(request->config);
    processingDirty_ = true;

    applyProcessing();

    initializeActiveContour();

    if (state_ != WorkerState::Ready)
        return;

    publish(true);

    initialShown_ = true;
}

void ImageProcessingWorker::initializeActiveContour()
{
    if (processedImage_.isNull())
        return;

    setState(WorkerState::Initializing);

    activeContour_.reset();
    resetMeasurement();

    assert(processedImage_.size() == config_.initialPhi.size());

    if (processedImage_.size() != config_.initialPhi.size())
    {
        config_.initialPhi = config_.initialPhi.scaled(
            processedImage_.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }

    assert(!config_.initialPhi.isNull());

    auto initialPhi = imageOwnerFromQImage(config_.initialPhi);

    fluvel_ip::ContourData initialCD(initialPhi.view(), config_.contourConfig.connectivity);

    bool is_rgb = (processedImage_.format() != QImage::Format_Grayscale8);

    processedOwner_ = imageOwnerFromQImage(processedImage_);

    if (is_rgb)
    {
        activeContour_ = std::make_unique<fluvel_ip::ActiveContour>(
            std::move(initialCD),
            std::make_unique<fluvel_ip::RegionColorSpeedModel>(config_.contourConfig.regionParams),
            config_.contourConfig.contourParams);
    }
    else
    {
        activeContour_ = std::make_unique<fluvel_ip::ActiveContour>(
            std::move(initialCD),
            std::make_unique<fluvel_ip::RegionGraySpeedModel>(config_.contourConfig.regionParams),
            config_.contourConfig.contourParams);
    }

    activeContour_->update(processedOwner_.view());

    setState(WorkerState::Ready);
    initialShown_ = false;
}

void ImageProcessingWorker::finish()
{
    setState(WorkerState::Finished);
}

void ImageProcessingWorker::finalizeAndPrepareNextRun()
{
    finish();

    publish(true);

    initializeActiveContour();
}

void ImageProcessingWorker::suspend()
{
    if (state_ == WorkerState::Running)
    {
        setState(WorkerState::Suspended);

        publish(true);
    }
}

void ImageProcessingWorker::resume()
{
    if (state_ == WorkerState::Suspended || state_ == WorkerState::Ready)
        start();
}

void ImageProcessingWorker::scheduleTick()
{
    if (tickScheduled_)
        return;

    tickScheduled_ = true;

    post([this] {
        tickScheduled_ = false;
        onTick();
    });
}

void ImageProcessingWorker::onTick()
{
    // A tick may still be queued after a pause or a new initialization.
    if (state_ != WorkerState::Running || !activeContour_)
        return;

    fluvel_ip::ElapsedTimer timeSliceBudget;
    timeSliceBudget.start();

    while (timeSliceBudget.elapsedLessThan(timeSliceMs_))
    {
        if (!stepOnceAlgo())
        {
            finalizeAndPrepareNextRun();
            return;
        }
    }

    if (mode_ == RunMode::Interactive)
        publish(false);

    // Yield to the event loop so that pending commands are handled.
    scheduleTick();
}

void ImageProcessingWorker::publish(bool force)
{
    if (!activeContour_)
        return;

    const auto now = Clock::now();

    if (!force)
    {
        const auto sinceLast = now - lastPublish_;

        if (sinceLast < kMinPublishIntervalMs)
            return;

        // The consumer is still busy with the previous frame: keep computing
        // and publish a more recent one later.
        if (frameInFlight_.load(std::memory_order_acquire) && sinceLast < kAckTimeoutMs)
            return;
    }

    lastPublish_ = now;
    frameInFlight_.store(true, std::memory_order_release);

    updateDiagnostics();
    emitContour();
}

void ImageProcessingWorker::updateDiagnostics()
{
    if (!activeContour_)
        return;

    fluvel_ip::ContourDiagnostics diag;

    if (isMeasuring_)
        diag.elapsedSec = measurementTimer_.elapsedSec();
    else
        diag.elapsedSec = 0.0;

    activeContour_->fillDiagnostics(diag);

    emit diagnosticsUpdated(diag);
}

void ImageProcessingWorker::emitContour()
{
    if (!activeContour_)
        return;

    emit contourUpdated(activeContour_->exportOuterBoundary(),
                        activeContour_->exportInnerBoundary());
}

void ImageProcessingWorker::setMode(RunMode mode)
{
    mode_ = mode;

    if (mode_ == RunMode::Interactive)
        timeSliceMs_ = kTimeSliceInteractiveMs;
    else if (mode_ == RunMode::Converge)
        timeSliceMs_ = kTimeSliceConvergeMs;
}

void ImageProcessingWorker::setState(WorkerState state)
{
    state_ = state;
    emit stateChanged(state);
}

void ImageProcessingWorker::resetMeasurement()
{
    isMeasuring_ = false;
}

} // namespace fluvel
