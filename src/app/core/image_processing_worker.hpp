// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>

#ifndef Q_MOC_RUN

#include "active_contour.hpp"
#include "application_settings_types.hpp"
#include "contour_diagnostics.hpp"
#include "elapsed_timer.hpp"
#include "image_owner.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

#endif

namespace fluvel
{

/**
 * @brief Execution mode of the worker.
 *
 * - Interactive: periodic updates with UI refresh.
 * - Converge   : runs as fast as possible until completion, without UI updates.
 */
enum class RunMode
{
    Interactive, ///< UI updates enabled
    Converge     ///< Full-speed execution without intermediate UI updates
};

/**
 * @brief Internal state of the ImageProcessingWorker.
 */
enum class WorkerState
{
    Uninitialized, ///< Worker has not been initialized
    Initializing,  ///< Preparing active contour
    Ready,         ///< Ready to start processing
    Suspended,     ///< Paused state
    Running,       ///< Processing is ongoing (tick chain active)
    Finished       ///< Processing finished and reset prepared for next run
};

/**
 * @brief Qt worker running image processing and segmentation in its own thread.
 *
 * The worker owns a QThread and lives in it. The public methods can be called
 * from any thread (typically the GUI thread): they only post a command to the
 * worker thread, so they return immediately and never block the interface.
 * Results are sent back through Qt signals, which are queued to the receivers.
 *
 * Execution is a chain of short ticks processed by the worker event loop, so
 * commands (pause, step, restart, ...) are handled between two ticks.
 *
 * - Interactive mode: the algorithm iterates as fast as it can. A result is
 *   published only when the previous one has been acknowledged by the consumer
 *   (see acknowledgeFrame()) and when a minimum interval has elapsed. Frames
 *   in between are skipped, so the consumer is never flooded and always
 *   receives the most recent contour ("Auto" display policy).
 * - Converge mode: the algorithm runs at full speed, without intermediate
 *   publications, and only the final result is published.
 *
 * Events that matter to the user (initial contour, pause, single step, end of
 * run) are always published, whatever the flow control state.
 *
 * Responsibilities:
 * - Manage image processing and segmentation lifecycle
 * - Control execution flow (interactive vs converge)
 * - Emit processed images and contour updates
 * - Provide diagnostics and performance measurements
 *
 * @note The preprocessing filters also run in the worker thread. Consecutive
 *       calls to initialize() are coalesced: only the latest request is run.
 *
 * @note A future "publish every iteration" policy would only need to change
 *       the condition in publish(): wait for an acknowledgement coming from
 *       the viewer instead of skipping frames.
 */
class ImageProcessingWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the worker and start its thread.
     */
    ImageProcessingWorker();

    /**
     * @brief Stop the worker thread and wait for its termination.
     */
    ~ImageProcessingWorker() override;

    ImageProcessingWorker(const ImageProcessingWorker&) = delete;
    ImageProcessingWorker& operator=(const ImageProcessingWorker&) = delete;

    /**
     * @brief Initialize the worker with an input image and configuration.
     *
     * Asynchronous. If several requests are pending, only the latest is run.
     *
     * @param image Input image.
     * @param config Processing configuration.
     */
    void initialize(const QImage& image, const ImageComputeConfig& config);

    /**
     * @brief Restart processing from scratch.
     *
     * Resets internal state and starts execution.
     */
    void restart();

    /**
     * @brief Toggle pause/resume state.
     */
    void togglePause();

    /**
     * @brief Execute a single iteration of the algorithm.
     */
    void step();

    /**
     * @brief Run the algorithm until convergence.
     *
     * Disables intermediate UI updates and emits only the final result.
     */
    void converge();

    /**
     * @brief Tell the worker that the last published contour has been consumed.
     *
     * Thread-safe, can be called from any thread. Allows the worker to publish
     * the next interactive frame.
     */
    void acknowledgeFrame() noexcept;

signals:
    /**
     * @brief Emitted when a processed image is available.
     */
    void processedImageReady(const QImage& img);

    /**
     * @brief Emitted when contour geometry is updated.
     */
    void contourUpdated(const fluvel_ip::Contour& outerContour,
                        const fluvel_ip::Contour& innerContour);

    /**
     * @brief Emitted when the worker state changes.
     */
    void stateChanged(fluvel::WorkerState state);

    /**
     * @brief Emitted when diagnostics are updated.
     */
    void diagnosticsUpdated(fluvel_ip::ContourDiagnostics diag);

private:
    /// Pending initialization request (latest one wins).
    struct InitRequest
    {
        QImage image;
        ImageComputeConfig config;
    };

    using Clock = std::chrono::steady_clock;

    // Command implementations, always executed in the worker thread.
    void initializeImpl();
    void restartImpl();
    void togglePauseImpl();
    void stepImpl();
    void convergeImpl();

    /// Run a command in the worker thread.
    template <typename F>
    void post(F&& f);

    /// Schedule the next tick (at most one is pending).
    void scheduleTick();

    /// One tick of the execution loop.
    void onTick();

    /**
     * @brief Publish diagnostics and contour.
     * @param force Bypass the flow control (user visible events).
     */
    void publish(bool force);

    /// Emit current contour data.
    void emitContour();

    /// Update diagnostics structure.
    void updateDiagnostics();

    /// Suspend execution.
    void suspend();

    /// Resume execution.
    void resume();

    /// Start execution.
    void start();

    /// Perform one processing step (wrapper).
    void performStep();

    /**
     * @brief Execute one algorithm step.
     * @return true if the algorithm should continue, false if finished.
     */
    bool stepOnceAlgo();

    /// Apply pre-processing pipeline.
    void applyProcessing();

    /// Initialize the active contour instance.
    void initializeActiveContour();

    /// Finalize current run and prepare next execution.
    void finalizeAndPrepareNextRun();

    /// Finish execution.
    void finish();

    /// Set execution mode.
    void setMode(RunMode mode);

    /// Set worker state and emit signal.
    void setState(WorkerState state);

    /// Reset performance measurement.
    void resetMeasurement();

    // Shared between threads.
    QThread thread_;
    QMutex requestMutex_;
    std::optional<InitRequest> pendingInit_;
    std::atomic<bool> frameInFlight_{false};

    // Used by the worker thread only.
    WorkerState state_{WorkerState::Uninitialized};
    RunMode mode_{RunMode::Interactive};
    std::unique_ptr<fluvel_ip::ActiveContour> activeContour_;

    QImage image_;
    QImage processedImage_;
    fluvel_ip::ImageOwner processedOwner_;

    /// Time slice of one tick for converge mode.
    static constexpr std::chrono::milliseconds kTimeSliceConvergeMs{15};

    /// Time slice of one tick for interactive mode.
    static constexpr std::chrono::milliseconds kTimeSliceInteractiveMs{10};

    /// Minimum delay between two published frames (~60 FPS). Bounds the
    /// conversion work done by the consumer.
    static constexpr std::chrono::milliseconds kMinPublishIntervalMs{16};

    /// Safety net: publish anyway if a frame has not been acknowledged.
    static constexpr std::chrono::milliseconds kAckTimeoutMs{250};

    std::chrono::milliseconds timeSliceMs_{kTimeSliceInteractiveMs};
    Clock::time_point lastPublish_{};
    bool tickScheduled_{false};

    bool initialShown_{false};

    ImageComputeConfig config_{};

    fluvel_ip::ContourDiagnostics diag_;
    fluvel_ip::ElapsedTimer measurementTimer_;
    bool isMeasuring_{false};

    bool processingDirty_{true};
};

} // namespace fluvel
