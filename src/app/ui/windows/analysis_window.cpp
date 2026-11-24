// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "analysis_window.hpp"

#include "analysis_widget.hpp"
#include "animated_push_button.hpp"
#include "icon_loader.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

#include <utility>

namespace fluvel
{

AnalysisWindow::AnalysisWindow(QWidget* parent)
    : QMainWindow(parent)

{
    QSettings settings;

    setWindowTitle(tr("Analysis: Input"));

    if (settings.contains("ui_geometry/analysis_window"))
    {
        restoreGeometry(settings.value("ui_geometry/analysis_window").toByteArray());
    }
    else
    {
        resize(900, 600);
    }

    ///////////////////////////////////////////////////////////////
    ///          Input analysis QMainWindow (this)              ///
    ///////////////////////////////////////////////////////////////

    widget1_ = new AnalysisWidget(this);
    widget2_ = new AnalysisWidget(this);

    QHBoxLayout* listsSelectLayout = new QHBoxLayout;
    listsSelectLayout->addWidget(widget1_);
    listsSelectLayout->addWidget(widget2_);

    QIcon measureIcon = il::loadIcon(":/icons/actions/measure-symbolic.svg");

    computeButton_ = new AnimatedPushButton(tr("Compute Hausdorff Distance"));
    computeButton_->setEnabled(false);
    computeButton_->setIcon(measureIcon);

    alignCentroidsCheck_ = new QCheckBox(tr("Align centroids"));
    alignCentroidsCheck_->setChecked(
        settings.value("analysis_window/align_centroids", false).toBool());
    alignCentroidsCheck_->setToolTip(
        tr("Unchecked: distances are measured between the shapes as they are positioned in the "
           "images, so a shift between them increases the distance.\n"
           "Checked: the centroids of both shapes are superimposed first, so only the difference "
           "of shape is measured (the shift is given by the distance between centroids).\n"
           "Click on Compute again to apply the change."));

    QVBoxLayout* inputLayout = new QVBoxLayout;
    inputLayout->addLayout(listsSelectLayout);
    inputLayout->addWidget(alignCentroidsCheck_);
    inputLayout->addWidget(computeButton_);

    connect(computeButton_, &QPushButton::clicked, this, &AnalysisWindow::computeHd);
    // The displayed results no longer match the options once they change.
    connect(alignCentroidsCheck_, &QCheckBox::toggled, this, &AnalysisWindow::checkLists);

    connect(&worker_, &AnalysisWorker::analysisReady, this, &AnalysisWindow::onAnalysisReady);

    ///////////////////////////////////////
    ///////////////////////////////////////
    ///////////////////////////////////////

    ///////////////////////////////////////
    ///          result_popup           ///
    ///////////////////////////////////////

    hausdorffLabel_ = new QLabel(this);
    hausdorffLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hausdorffLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    hausdorffLabel_->setToolTip(tr("Maximum of the minimum distances between the two shapes."));

    normalizedHausdorffLabel_ = new QLabel(this);
    normalizedHausdorffLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    normalizedHausdorffLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    normalizedHausdorffLabel_->setToolTip(
        tr("Hausdorff distance normalized by the largest image diagonal."));

    QVBoxLayout* hausdorffLayout = new QVBoxLayout;
    hausdorffLayout->addWidget(hausdorffLabel_);
    hausdorffLayout->addWidget(normalizedHausdorffLabel_);
    QGroupBox* hausdorffGroup = new QGroupBox(tr("Hausdorff measure"));
    hausdorffGroup->setLayout(hausdorffLayout);

    percentileSp_ = new QSpinBox;
    percentileSp_->setSingleStep(1);
    percentileSp_->setMinimum(0);
    percentileSp_->setMaximum(100);
    percentileSp_->setSuffix(" %");
    percentileSp_->setValue(95);
    QFormLayout* percentileLayout = new QFormLayout;
    percentileLayout->addRow(tr("Percentile: "), percentileSp_);

    connect(percentileSp_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &AnalysisWindow::refreshQuantile);

    quantileLabel_ = new QLabel(this);
    quantileLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    quantileLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    quantileLabel_->setToolTip(
        tr("Uses a percentile of the minimum distances instead of the maximum minimum distance."));

    normalizedQuantileLabel_ = new QLabel(this);
    normalizedQuantileLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    normalizedQuantileLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    normalizedQuantileLabel_->setToolTip(
        tr("Quantile-based Hausdorff distance normalized by the largest image diagonal."));

    QVBoxLayout* quantileLayout = new QVBoxLayout;
    quantileLayout->addLayout(percentileLayout);
    quantileLayout->addWidget(quantileLabel_);
    quantileLayout->addWidget(normalizedQuantileLabel_);
    QGroupBox* quantileGroup = new QGroupBox(tr("Hausdorff quantile measure"));
    quantileGroup->setLayout(quantileLayout);

    centroidsDistLabel_ = new QLabel(this);
    centroidsDistLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    centroidsDistLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    normalizedCentroidsDistLabel_ = new QLabel(this);
    normalizedCentroidsDistLabel_->setToolTip(
        tr("Distance between centroids normalized by the largest image diagonal."));
    normalizedCentroidsDistLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    normalizedCentroidsDistLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* centroidsLayout = new QVBoxLayout;
    centroidsLayout->addWidget(centroidsDistLabel_);
    centroidsLayout->addWidget(normalizedCentroidsDistLabel_);
    QGroupBox* centroidsGroup = new QGroupBox(tr("Shape distances"));
    centroidsGroup->setLayout(centroidsLayout);

    timeLabel_ = new QLabel(this);
    timeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    timeLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* timeLayout = new QVBoxLayout;
    timeLayout->addWidget(timeLabel_);
    QGroupBox* timeGroup = new QGroupBox(tr("Time"));
    timeGroup->setLayout(timeLayout);

    QVBoxLayout* resultsLayout = new QVBoxLayout;
    resultsLayout->addWidget(hausdorffGroup);
    resultsLayout->addWidget(quantileGroup);
    resultsLayout->addWidget(centroidsGroup);
    resultsLayout->addWidget(timeGroup);
    resultsLayout->addStretch(1);

    ///////////////////////////////////////
    ///////////////////////////////////////
    ///////////////////////////////////////

    checkLists();

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(inputLayout);
    setCentralWidget(centralWidget);

    resultsPopup_ = new QDialog(this);
    resultsPopup_->setWindowTitle(tr("Analysis: Results"));
    resultsPopup_->setLayout(resultsLayout);
}

void AnalysisWindow::computeHd()
{
    if (!widget1_->shape().isValid() || !widget2_->shape().isValid())
        return;

    AnalysisRequest request;
    request.id = ++lastRequestId_;
    request.shape1 = widget1_->shape();
    request.shape2 = widget2_->shape();
    request.image1 = widget1_->image();
    request.image2 = widget2_->image();
    request.rgb1 = widget1_->rgb();
    request.rgb2 = widget2_->rgb();
    request.size1 = QSize(widget1_->imageWidth(), widget1_->imageHeight());
    request.size2 = QSize(widget2_->imageWidth(), widget2_->imageHeight());
    request.options = {.alignCentroids = alignCentroidsCheck_->isChecked()};

    computing_ = true;
    showPlaceholders(tr("..."));
    computeButton_->setEnabled(false);
    computeButton_->setText(tr("Computing..."));

    worker_.submit(std::move(request));
}

void AnalysisWindow::onAnalysisReady(const AnalysisResult& result)
{
    computing_ = false;
    computeButton_->setText(tr("Compute Hausdorff Distance"));

    // The selection changed while the computation was running.
    if (result.id != lastRequestId_)
    {
        checkLists();
        return;
    }

    checkLists();

    data_ = result.data;
    factor_ = result.normalizationFactor;

    const float hausdorffDist = result.hausdorffDistance;
    const float hausdorffQuantile = data_->statistics->quantileDistance(percentileSp_->value());
    const float centroidsGap = result.centroidsDistance;

    const float hdRatio = factor_ * hausdorffDist;
    const float quantileRatio = factor_ * hausdorffQuantile;
    const float gapRatio = factor_ * centroidsGap;

    // =========================
    // Hausdorff
    // =========================

    hausdorffLabel_->setText(
        tr("Hausdorff distance: %1 px").arg(QString::number(hausdorffDist, 'f', 2)));

    normalizedHausdorffLabel_->setText(
        tr("Normalized Hausdorff distance: %1 %").arg(QString::number(hdRatio, 'f', 2)));

    // =========================
    // Quantile
    // =========================

    quantileLabel_->setText(
        tr("Hausdorff quantile: %1 px").arg(QString::number(hausdorffQuantile, 'f', 2)));

    normalizedQuantileLabel_->setText(
        tr("Normalized Hausdorff quantile: %1 %").arg(QString::number(quantileRatio, 'f', 2)));

    // =========================
    // Centroids
    // =========================

    centroidsDistLabel_->setText(
        tr("Distance between centroids: %1 px").arg(QString::number(centroidsGap, 'f', 2)));

    normalizedCentroidsDistLabel_->setText(
        tr("Normalized distance between centroids: %1 %").arg(QString::number(gapRatio, 'f', 2)));

    // =========================
    // Time
    // =========================

    timeLabel_->setText(
        tr("Computation time: %1 s").arg(QString::number(result.elapsedSec, 'f', 2)));

    resultsPopup_->show();
}

void AnalysisWindow::refreshQuantile(int hundredth)
{
    if (data_ != nullptr)
    {
        const float hausdorffQuantile = data_->statistics->quantileDistance(hundredth);
        const float quantileRatio = factor_ * hausdorffQuantile;

        quantileLabel_->setText(
            tr("Hausdorff quantile: %1 px").arg(QString::number(hausdorffQuantile, 'f', 2)));

        normalizedQuantileLabel_->setText(
            tr("Normalized Hausdorff quantile: %1 %").arg(QString::number(quantileRatio, 'f', 2)));
    }
}

void AnalysisWindow::showPlaceholders(const QString& mark)
{
    data_.reset();

    hausdorffLabel_->setText(tr("Hausdorff distance: %1 px").arg(mark));
    normalizedHausdorffLabel_->setText(tr("Normalized Hausdorff distance: %1 %").arg(mark));
    quantileLabel_->setText(tr("Hausdorff quantile: %1 px").arg(mark));
    normalizedQuantileLabel_->setText(tr("Normalized Hausdorff quantile: %1 %").arg(mark));
    centroidsDistLabel_->setText(tr("Distance between centroids: %1 px").arg(mark));
    normalizedCentroidsDistLabel_->setText(
        tr("Normalized distance between centroids: %1 %").arg(mark));
    timeLabel_->setText(tr("Computation time: %1 s").arg(mark));
}

void AnalysisWindow::checkLists()
{
    // Whatever is displayed was computed for another selection or other options.
    showPlaceholders(QStringLiteral("--"));

    const bool valid = widget1_->shape().isValid() && widget2_->shape().isValid();

    // A result still to come would concern the previous selection.
    if (computing_)
        ++lastRequestId_;

    computeButton_->setEnabled(valid && !computing_);
}

void AnalysisWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;

    settings.setValue("ui_geometry/analysis_window", saveGeometry());
    settings.setValue("analysis_window/align_centroids", alignCentroidsCheck_->isChecked());

    widget1_->saveSettings();
    widget2_->saveSettings();

    QMainWindow::closeEvent(event);
}

} // namespace fluvel
