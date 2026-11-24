// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QImage>
#include <QWidget>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QShowEvent;
class QTimer;

namespace fluvel
{

class HistogramWidget;

/**
 * @brief Window showing the histogram of the image displayed in the viewer.
 *
 * The window is fed with the image sent to the viewer (setImage()). While it is hidden
 * nothing is computed: the last image is only kept, and the histogram is computed when
 * the window is shown.
 *
 * While the window is visible and the viewer receives many images in a row (interactive
 * segmentation), the computation is coalesced: only the latest image is processed, at most
 * once every few tens of milliseconds, so the viewer is never slowed down.
 */
class HistogramWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit HistogramWindow(QWidget* parent = nullptr);

    /**
     * @brief Gives the window the image currently displayed.
     *
     * The image is shared, not copied. A null image clears the histogram.
     */
    void setImage(const QImage& image);

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    /// Computes the histogram of the latest image and displays it.
    void refresh();

    HistogramWidget* histogramWidget_{nullptr};
    QCheckBox* logCheck_{nullptr};
    QLabel* infoLabel_{nullptr};

    QTimer* refreshTimer_{nullptr};

    /// Latest image received (shared with the viewer).
    QImage image_;

    /// True if image_ has not been processed yet.
    bool dirty_{false};
};

} // namespace fluvel
