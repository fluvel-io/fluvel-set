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

#include <algorithm>
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

    // Child of this: moved to the worker thread with it.
    tickTimer_ = new QTimer(this);
    tickTimer_->setSingleShot(true);
    tickTimer_->setTimerType(Qt::PreciseTimer);
    connect(tickTimer_, &QTimer::timeout, this, &ImageProcessingWorker::onTick);

    moveToThread(&thread_);
    thread_.start();
}

ImageProcessingWorker::~ImageProcessingWorker()
{
    // No new tick from now on, then stop the timer from its own thread.
    shuttingDown_.store(true, std::memory_order_release);

    if (thread_.isRunning())
    {
        QMetaObject::invokeMethod(
            this, [this] { tickTimer_->stop(); }, Qt::BlockingQueuedConnection);
    }

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

quint64 ImageProcessingWorker::initialize(const QImage& image, const ImageComputeConfig& config)
{
    const quint64 generation = ++initCount_;

    {
        const QMutexLocker lock(&requestMutex_);
        pendingInit_ = InitRequest{image, config, generation};
    }

    post([this] { initializeImpl(); });

    return generation;
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

    publish();

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
            publish();

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
            publish();

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
        publish();
    }
}

void ImageProcessingWorker::converge()
{
    if (state_ == WorkerState::Uninitialized || state_ == WorkerState::Initializing)
        return;

    if (state_ == WorkerState::Ready)
    {
        publish();

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
            computeSec_ = 0.0;
        }

        fluvel_ip::ElapsedTimer iterationTimer;
        iterationTimer.start();

        activeContour_->step();

        computeSec_ += iterationTimer.elapsedSec();
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
        emit processedImageReady(processedImage_, generation_);

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
    generation_ = request->generation;
    processingDirty_ = true;

    applyProcessing();

    initializeActiveContour();

    if (state_ != WorkerState::Ready)
        return;

    publish();

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

    publish();

    initializeActiveContour();
}

void ImageProcessingWorker::suspend()
{
    if (state_ == WorkerState::Running)
    {
        setState(WorkerState::Suspended);

        publish();
    }
}

void ImageProcessingWorker::resume()
{
    if (state_ == WorkerState::Suspended || state_ == WorkerState::Ready)
        start();
}

void ImageProcessingWorker::scheduleTick(std::chrono::milliseconds delay)
{
    if (tickScheduled_ || shuttingDown_.load(std::memory_order_acquire))
        return;

    tickScheduled_ = true;

    if (delay.count() > 0)
        tickTimer_->start(delay);
    else
        post([this] { onTick(); });
}

void ImageProcessingWorker::onTick()
{
    tickScheduled_ = false;

    // A tick may still be pending after a pause or a new initialization.
    if (shuttingDown_.load(std::memory_order_acquire) || state_ != WorkerState::Running ||
        !activeContour_)
        return;

    if (mode_ == RunMode::Converge)
    {
        // Full speed: no pacing, no intermediate publication.
        fluvel_ip::ElapsedTimer budget;
        budget.start();

        while (budget.elapsedLessThan(kTimeSliceConvergeMs))
        {
            if (!stepOnceAlgo())
            {
                finalizeAndPrepareNextRun();
                return;
            }
        }

        // Yield to the event loop so that pending commands are handled.
        scheduleTick();
        return;
    }

    // Interactive: one frame every kFrameIntervalMs, at most
    // kIterationsPerFrame iterations per frame.
    const auto now = Clock::now();
    const auto sinceLast = now - lastPublish_;

    // Frames are paced from start to start: the computation time is part of
    // the frame interval.
    if (now < nextFrameDue_)
    {
        scheduleTick(std::chrono::duration_cast<std::chrono::milliseconds>(nextFrameDue_ - now) +
                     std::chrono::milliseconds{1});
        return;
    }

    // Never compute ahead of the consumer.
    if (frameInFlight_.load(std::memory_order_acquire) && sinceLast < kAckTimeoutMs)
    {
        scheduleTick(kAckPollMs);
        return;
    }

    // If a frame took longer than the interval, do not try to catch up.
    nextFrameDue_ = std::max(nextFrameDue_ + kFrameIntervalMs, now);

    for (int i = 0; i < kIterationsPerFrame; ++i)
    {
        if (!stepOnceAlgo())
        {
            finalizeAndPrepareNextRun();
            return;
        }
    }

    publish(true);
    scheduleTick();
}

void ImageProcessingWorker::publish(bool streaming)
{
    if (!activeContour_)
        return;

    lastPublish_ = Clock::now();
    frameInFlight_.store(true, std::memory_order_release);

    updateDiagnostics(streaming);
    emitContour();
}

void ImageProcessingWorker::updateDiagnostics(bool streaming)
{
    if (!activeContour_)
        return;

    fluvel_ip::ContourDiagnostics diag;

    if (isMeasuring_)
        diag.elapsedSec = computeSec_;
    else
        diag.elapsedSec = 0.0;

    activeContour_->fillDiagnostics(diag);

    emit diagnosticsUpdated(diag, streaming);
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
}

void ImageProcessingWorker::setState(WorkerState state)
{
    state_ = state;
    emit stateChanged(state);
}

void ImageProcessingWorker::resetMeasurement()
{
    isMeasuring_ = false;
    computeSec_ = 0.0;
}

} // namespace fluvel
