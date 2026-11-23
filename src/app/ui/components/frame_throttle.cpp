// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "frame_throttle.hpp"

#include <algorithm>
#include <limits>

namespace fluvel
{

FrameThrottle::FrameThrottle(QObject* parent) : QObject(parent)
{
    sinceLastFrame_.start();

    timer_.setSingleShot(true);

    connect(&timer_, &QTimer::timeout, this,
            [this]()
            {
                if (hasPending_)
                    emitPending();
            });
}

void FrameThrottle::setMaxFps(double fps)
{
    minIntervalMs_ = (fps <= 0.0) ? 0 : static_cast<int>(1000.0 / fps);
}

void FrameThrottle::submit(const DisplayFrame& frame)
{
    pending_ = frame;
    hasPending_ = true;

    if (minIntervalMs_ == 0 || sinceLastFrame_.elapsed() >= minIntervalMs_)
    {
        emitPending();
        return;
    }

    if (timer_.isActive())
        return;

    const qint64 remaining = minIntervalMs_ - sinceLastFrame_.elapsed();

    if (remaining > 0)
        timer_.start(static_cast<int>(std::min<qint64>(remaining, std::numeric_limits<int>::max())));
}

void FrameThrottle::emitPending()
{
    const DisplayFrame frame = pending_;
    hasPending_ = false;

    emit frameReady(frame);

    sinceLastFrame_.restart();
}

} // namespace fluvel
