// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QThread>

#ifndef Q_MOC_RUN

#include "color.hpp"
#include "hausdorff_distance.hpp"
#include "hausdorff_statistics.hpp"
#include "point_containers.hpp"
#include "shape.hpp"

#include <memory>
#include <optional>

#endif

namespace fluvel
{

/**
 * @brief Input of an analysis: the two shapes to compare and how to measure them.
 */
struct AnalysisRequest
{
    quint64 id{0}; ///< Identifier returned with the result.

    fluvel_ip::Shape shape1;
    fluvel_ip::Shape shape2;

    /// Images the shapes were extracted from, and the color of the shape in each.
    QImage image1;
    QImage image2;
    fluvel_ip::Rgb_uc rgb1{};
    fluvel_ip::Rgb_uc rgb2{};

    /// Sizes of the images, used to normalize the distances.
    QSize size1;
    QSize size2;

    fluvel_ip::HausdorffOptions options{};
};

/**
 * @brief Measures computed by an analysis.
 *
 * The statistics object is shared and read-only, so the receiver can compute
 * other quantiles without asking the worker again.
 */
struct AnalysisResult
{
    quint64 id{0};

    float hausdorffDistance{0.f};
    float centroidsDistance{0.f};

    /// Factor converting a distance in pixels into a percentage of the largest
    /// image diagonal.
    float normalizationFactor{0.f};

    /// Duration of the computation, in seconds.
    double elapsedSec{0.0};

    /// Owns everything the statistics depend on.
    struct Data
    {
        fluvel_ip::PointSet intersection;
        std::unique_ptr<fluvel_ip::HausdorffDistance> distance;
        std::unique_ptr<fluvel_ip::HausdorffStatistics> statistics;
    };

    std::shared_ptr<const Data> data;
};

/**
 * @brief Computes the shape analysis (intersection, Hausdorff distance,
 *        statistics, centroids) in its own thread.
 *
 * submit() returns immediately. If several requests arrive while a
 * computation is running, only the latest one is run. Each result carries the
 * identifier of its request, so the receiver can ignore an outdated one.
 *
 * @note submit() can be called from any thread. The result signal is queued to
 *       the receivers. The destructor waits for the computation in progress.
 */
class AnalysisWorker : public QObject
{
    Q_OBJECT

public:
    AnalysisWorker();
    ~AnalysisWorker() override;

    AnalysisWorker(const AnalysisWorker&) = delete;
    AnalysisWorker& operator=(const AnalysisWorker&) = delete;

    /// Ask for a new analysis.
    void submit(AnalysisRequest request);

signals:
    /// Emitted when an analysis is done.
    void analysisReady(fluvel::AnalysisResult result);

private:
    /// Run the latest pending request (worker thread).
    void run();

    /// Points of the smaller shape that have the color of the shape in the other image.
    static fluvel_ip::PointSet computeIntersection(const AnalysisRequest& request);

    QThread thread_;
    QMutex mutex_;
    std::optional<AnalysisRequest> pending_;
};

} // namespace fluvel
