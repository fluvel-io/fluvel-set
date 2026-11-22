// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "snapshot_worker.hpp"

#include "file_utils.hpp"

namespace fluvel
{

SnapshotWorker::SnapshotWorker()
{
    workerThread_ = std::thread(&SnapshotWorker::processQueue, this);
}

SnapshotWorker::~SnapshotWorker()
{
    {
        QMutexLocker locker(&mutex_);
        stopping_ = true;
    }

    condition_.wakeOne();

    if (workerThread_.joinable())
        workerThread_.join();
}

bool SnapshotWorker::enqueue(const QImage& image, const SnapshotPreferences& preferences)
{
    {
        QMutexLocker locker(&mutex_);

        if (stopping_)
            return false;

        if (requestQueue_.size() >= kMaxPendingSnapshots)
            return false;

        requestQueue_.push_back(Request{image, preferences});
    }

    condition_.wakeOne();

    return true;
}

void SnapshotWorker::processQueue()
{
    for (;;)
    {
        Request request;

        {
            QMutexLocker locker(&mutex_);

            while (requestQueue_.empty() && !stopping_)
                condition_.wait(&mutex_);

            if (requestQueue_.empty() && stopping_)
                break;

            request = std::move(requestQueue_.front());
            requestQueue_.pop_front();
        }

        saveImage(request);
    }
}

void SnapshotWorker::saveImage(const Request& request)
{
    const SnapshotPreferences& preferences = request.preferences;

    const QString fileName = file_utils::buildOutputFileName(
        preferences.directory, preferences.baseName,
        QString::fromLatin1(preferences.preferredFormat), preferences.appendTimestamp);

    if (!request.image.save(fileName, preferences.preferredFormat.constData()))
    {
        emit snapshotError(tr("Failed to save snapshot: %1").arg(fileName));
        return;
    }

    emit snapshotSaved(fileName);
}

} // namespace fluvel