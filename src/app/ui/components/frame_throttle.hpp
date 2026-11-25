// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "frame_pipeline.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace fluvel
{

/**
 * @brief Limits the rate at which frames are displayed.
 *
 * Frames are submitted as fast as they are produced. A frame is emitted at
 * once if the last one is old enough; otherwise it is kept, and emitted when
 * the minimum interval has elapsed. A frame that arrives while another one is
 * waiting replaces it: only the most recent frame is ever displayed, so the
 * display never lags behind the source.
 *
 * The class knows nothing about the viewer: it only delays DisplayFrame
 * objects.
 */
class FrameThrottle : public QObject
{
    Q_OBJECT

public:
    explicit FrameThrottle(QObject* parent = nullptr);

    /**
     * @brief Sets the maximum display rate.
     *
     * @param fps Maximum number of frames per second. A value <= 0 disables the limit.
     */
    void setMaxFps(double fps);

    /**
     * @brief Submits a frame.
     *
     * The frame is emitted immediately or later, or dropped if a more recent
     * frame is submitted before the interval has elapsed.
     */
    void submit(const DisplayFrame& frame);

signals:
    /**
     * @brief Emitted when a frame must be displayed.
     */
    void frameReady(const fluvel::DisplayFrame& frame);

private:
    void emitPending();

    DisplayFrame pending_;
    bool hasPending_{false};

    QElapsedTimer sinceLastFrame_;
    int minIntervalMs_{0};

    QTimer timer_;
};

} // namespace fluvel
