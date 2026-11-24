// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "hausdorff_widget.hpp"
#include "hausdorff_worker.hpp"

#include <QMainWindow>

class QWidget;
class QCloseEvent;
class QLabel;
class QSpinBox;
class QCheckBox;
class QDialog;

namespace fluvel
{

class HausdorffWidget;
class AnimatedPushButton;

/**
 * @brief Main window for comparing two shapes with the Hausdorff distance.
 *
 * This window allows the user to compare shapes extracted from two
 * HausdorffWidget instances. It computes various metrics such as:
 * - Hausdorff distance
 * - quantile distance
 * - centroid distance
 * - shape intersection
 *
 * Results are displayed in a dedicated popup.
 *
 * The measures are computed in a separate thread (see HausdorffWorker), so the
 * window stays responsive with large shapes. While a computation is running,
 * the button is disabled and shows that the result is awaited.
 */
class HausdorffWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the Hausdorff window.
     *
     * @param parent Optional parent widget.
     */
    HausdorffWindow(QWidget* parent = nullptr);
    void checkLists();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    /// Ask the worker for the measures of the two selected shapes.
    void computeHd();

    /// Display the measures computed by the worker.
    void onHausdorffReady(const HausdorffResult& result);

    void refreshQuantile(int hundredth);

    /// Replace every displayed result by a mark ("--": none, "...": being computed).
    void showPlaceholders(const QString& mark);

    HausdorffWidget* widget1_{nullptr};
    HausdorffWidget* widget2_{nullptr};
    AnimatedPushButton* computeButton_{nullptr};
    QCheckBox* alignCentroidsCheck_{nullptr};

    QDialog* resultsPopup_{nullptr};
    QLabel* hausdorffLabel_{nullptr};
    QSpinBox* percentileSp_{nullptr};
    QLabel* quantileLabel_{nullptr};
    QLabel* normalizedHausdorffLabel_{nullptr};
    QLabel* normalizedQuantileLabel_{nullptr};
    QLabel* centroidsDistLabel_{nullptr};
    QLabel* normalizedCentroidsDistLabel_{nullptr};
    QLabel* timeLabel_{nullptr};

    /// Computes the measures in its own thread.
    HausdorffWorker worker_;

    /// Identifier of the latest request, to ignore an outdated result.
    quint64 lastRequestId_{0};

    /// Whether a computation is running.
    bool computing_{false};

    /// Statistics of the last result, to compute other quantiles.
    std::shared_ptr<const HausdorffResult::Data> data_;
    float factor_{0.f};
};

} // namespace fluvel
