// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>

#ifndef Q_MOC_RUN

#include "application_settings_types.hpp"

#include <optional>

#endif

namespace fluvel
{

/**
 * @brief Computes the settings preview (downscale + filters) in its own thread.
 *
 * submit() returns immediately. If several requests arrive while a
 * computation is running, only the latest one is run: the others are dropped,
 * so dragging a slider never builds a queue of outdated computations.
 *
 * Each result carries the identifier of the request it answers, so the
 * receiver can ignore a result that is already outdated.
 *
 * @note submit() can be called from any thread. The result signal is queued
 *       to the receivers.
 */
class ImagePreviewWorker : public QObject
{
    Q_OBJECT

public:
    ImagePreviewWorker();

    /// Stop the thread. Waits for the computation in progress, if any.
    ~ImagePreviewWorker() override;

    ImagePreviewWorker(const ImagePreviewWorker&) = delete;
    ImagePreviewWorker& operator=(const ImagePreviewWorker&) = delete;

    /**
     * @brief Ask for a new preview.
     *
     * @param id Identifier returned with the result.
     * @param input Original input image.
     * @param downscale Downscale parameters.
     * @param processing Filter parameters.
     */
    void submit(quint64 id, const QImage& input, const DownscaleParams& downscale,
                const fluvel_ip::ProcessingParams& processing);

signals:
    /**
     * @brief Emitted when a preview is computed.
     *
     * @param id Identifier of the request.
     * @param downscaled Downscaled image.
     * @param processed Filtered image (equal to @p downscaled if filtering failed).
     * @param elapsedSec Duration of the filter pipeline, in seconds.
     */
    void previewReady(quint64 id, const QImage& downscaled, const QImage& processed,
                      double elapsedSec);

private:
    struct Request
    {
        quint64 id{0};
        QImage input;
        DownscaleParams downscale;
        fluvel_ip::ProcessingParams processing;
    };

    /// Run the latest pending request (worker thread).
    void run();

    QThread thread_;
    QMutex mutex_;
    std::optional<Request> pending_;
};

} // namespace fluvel
