// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "analysis_worker.hpp"

#include "color_adapters.hpp"
#include "elapsed_timer.hpp"

#include <QMetaObject>
#include <QMutexLocker>

#include <algorithm>
#include <utility>

namespace fluvel
{

AnalysisWorker::AnalysisWorker()
    : QObject(nullptr)
{
    // Needed to queue the result across threads.
    qRegisterMetaType<fluvel::AnalysisResult>("fluvel::AnalysisResult");

    moveToThread(&thread_);
    thread_.start();
}

AnalysisWorker::~AnalysisWorker()
{
    thread_.quit();
    thread_.wait();
}

void AnalysisWorker::submit(AnalysisRequest request)
{
    {
        const QMutexLocker lock(&mutex_);
        pending_ = std::move(request);
    }

    QMetaObject::invokeMethod(this, [this] { run(); }, Qt::QueuedConnection);
}

fluvel_ip::PointSet AnalysisWorker::computeIntersection(const AnalysisRequest& request)
{
    const std::size_t size1 = request.shape1.points().size();
    const std::size_t size2 = request.shape2.points().size();

    // The smaller shape is tested against the image of the larger one.
    const bool firstIsSmaller = size1 < size2;

    const fluvel_ip::Shape& smallerShape = firstIsSmaller ? request.shape1 : request.shape2;
    const QImage& largerShapeImg = firstIsSmaller ? request.image2 : request.image1;
    const fluvel_ip::Rgb_uc& chosenRgb = firstIsSmaller ? request.rgb2 : request.rgb1;

    fluvel_ip::PointSet intersection;
    intersection.reserve(smallerShape.points().size());

    for (const auto& p : smallerShape.points())
    {
        if (p.x >= 0 && p.x < largerShapeImg.width() && p.y >= 0 && p.y < largerShapeImg.height())
        {
            if (toRgb_uc(largerShapeImg.pixel(p.x, p.y)) == chosenRgb)
                intersection.insert(p);
        }
    }

    return intersection;
}

void AnalysisWorker::run()
{
    std::optional<AnalysisRequest> request;

    {
        const QMutexLocker lock(&mutex_);
        request.swap(pending_);
    }

    // Already handled by a previous call: only the latest request is run.
    if (!request)
        return;

    fluvel_ip::ElapsedTimer timer;
    timer.start();

    auto data = std::make_shared<AnalysisResult::Data>();

    data->intersection = computeIntersection(*request);

    data->distance = std::make_unique<fluvel_ip::HausdorffDistance>(
        request->shape1, request->shape2, &data->intersection, request->options);

    data->statistics = std::make_unique<fluvel_ip::HausdorffStatistics>(
        request->shape1, request->shape2, &data->intersection, request->options);

    AnalysisResult result;
    result.id = request->id;
    result.elapsedSec = timer.elapsedSec();
    result.hausdorffDistance = data->distance->distance();
    result.centroidsDistance = data->distance->centroidsDistance();
    result.normalizationFactor =
        100.f / fluvel_ip::Shape::gridDiagonal(
                    std::max(request->size1.width(), request->size2.width()),
                    std::max(request->size1.height(), request->size2.height()));
    result.data = std::move(data);

    emit analysisReady(std::move(result));
}

} // namespace fluvel
