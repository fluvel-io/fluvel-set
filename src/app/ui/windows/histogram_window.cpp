// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "histogram_window.hpp"

#include "histogram.hpp"
#include "histogram_widget.hpp"
#include "image_adapters.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fluvel
{

namespace
{

/// Minimum delay between two computations while images keep arriving.
constexpr int kRefreshIntervalMs{100};

/// Above this number of pixels the histogram is sampled (see sampleStep()).
constexpr qint64 kFullResolutionPixels{25'000'000};

/// Sampling step keeping the computation time bounded on huge images.
int sampleStep(const QImage& image)
{
    const qint64 pixels = static_cast<qint64>(image.width()) * image.height();

    return 1 + static_cast<int>(pixels / kFullResolutionPixels);
}

bool isSupportedFormat(QImage::Format format)
{
    return format == QImage::Format_Grayscale8 || format == QImage::Format_RGB888 ||
           format == QImage::Format_ARGB32 || format == QImage::Format_RGB32;
}

} // namespace

HistogramWindow::HistogramWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("Histogram"));

    // A parent would make this a child widget: it must stay a window of its own.
    setWindowFlag(Qt::Window, true);

    QSettings settings;

    if (settings.contains("ui_geometry/histogram_window"))
        restoreGeometry(settings.value("ui_geometry/histogram_window").toByteArray());
    else
        resize(480, 420);

    histogramWidget_ = new HistogramWidget(this);

    logCheck_ = new QCheckBox(tr("Logarithmic scale"), this);
    logCheck_->setToolTip(tr("Makes the small values visible when a few levels dominate."));

    const bool logScale = settings.value("histogram_window/log_scale", false).toBool();
    logCheck_->setChecked(logScale);
    histogramWidget_->setLogScale(logScale);

    infoLabel_ = new QLabel(this);
    infoLabel_->setEnabled(false);

    QHBoxLayout* bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(logCheck_);
    bottomLayout->addStretch();
    bottomLayout->addWidget(infoLabel_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(histogramWidget_, 1);
    layout->addLayout(bottomLayout);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setSingleShot(true);
    refreshTimer_->setInterval(kRefreshIntervalMs);

    connect(refreshTimer_, &QTimer::timeout, this, &HistogramWindow::refresh);

    connect(logCheck_, &QCheckBox::toggled, histogramWidget_, &HistogramWidget::setLogScale);
}

void HistogramWindow::setImage(const QImage& image)
{
    image_ = image;
    dirty_ = true;

    // Hidden: nothing to compute. The window computes the histogram when it is shown.
    if (!isVisible())
        return;

    // Coalescing: the timer is not restarted if it is already running, so the latest image
    // is processed at most every kRefreshIntervalMs.
    if (!refreshTimer_->isActive())
        refreshTimer_->start();
}

void HistogramWindow::refresh()
{
    if (!dirty_)
        return;

    dirty_ = false;

    if (image_.isNull())
    {
        histogramWidget_->setHistogram({});
        infoLabel_->clear();
        return;
    }

    // Other formats (indexed, premultiplied, 16 bits...) are converted. This copy is rare.
    const QImage source =
        isSupportedFormat(image_.format()) ? image_ : image_.convertToFormat(QImage::Format_RGB32);

    const fluvel_ip::Histogram histogram =
        fluvel_ip::computeHistogram(imageViewFromQImage(source), sampleStep(source));

    histogramWidget_->setHistogram(histogram);

    infoLabel_->setText(tr("%1 × %2 px, %3")
                            .arg(source.width())
                            .arg(source.height())
                            .arg(histogram.color ? tr("color") : tr("grayscale")));
}

void HistogramWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // The image may have changed while the window was hidden.
    if (dirty_)
        refresh();
}

void HistogramWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;

    settings.setValue("ui_geometry/histogram_window", saveGeometry());
    settings.setValue("histogram_window/log_scale", logCheck_->isChecked());

    QWidget::closeEvent(event);
}

} // namespace fluvel
