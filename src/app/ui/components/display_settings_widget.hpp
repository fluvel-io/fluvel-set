// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#ifndef Q_MOC_RUN
#include "application_settings_types.hpp"
#endif

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

class QPropertyAnimation;
class QSlider;

namespace fluvel
{

class ColorSelectorWidget;

/**
 * @brief Widget for configuring display parameters of the image viewer.
 *
 * This widget provides UI controls to adjust how images and overlays
 * are rendered, including:
 * - pipeline source selection (raw / preprocessed)
 * - contour colors
 * - display options (flip, smoothing, overlay)
 * - brightness, contrast and auto-contrast of the displayed image (screen only)
 *
 * It emits displayConfigChanged() whenever the configuration is updated.
 */
class DisplaySettingsWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the display settings widget.
     *
     * @param config Initial display configuration.
     * @param parent Optional parent widget.
     */
    DisplaySettingsWidget(const DisplayConfig& config, QWidget* parent = nullptr);

    /**
     * @brief Updates the availability of image display modes.
     *
     * When preprocessing is unavailable, the display mode is forced
     * back to the source image.
     *
     * @param hasPreprocessing True if a preprocessed image is available.
     */
    void updateBackgroundAvailability(bool hasPreprocessing);

    /**
     * @brief Enables or disables the selfie mirror display.
     *
     * Updates the corresponding user interface control.
     * The usual configuration update chain is triggered through the
     * checkbox's toggled() signal.
     *
     * @param enabled True to enable the mirror effect.
     */
    void setMirrorModeEnabled(bool enabled);

    /**
     * @brief Enables or disables smooth image rendering.
     *
     * Updates the corresponding user interface control.
     * The usual configuration update chain is triggered through the
     * checkbox's toggled() signal.
     *
     * @param enabled True to enable smooth rendering.
     */
    void setSmoothDisplayEnabled(bool enabled);

    /**
     * @brief Shows or hides the algorithm overlay.
     *
     * Updates the corresponding user interface control.
     * The usual configuration update chain is triggered through the
     * checkbox's toggled() signal.
     *
     * @param enabled True to display the overlay.
     */
    void setAlgorithmOverlayEnabled(bool enabled);

signals:
    /**
     * @brief Emitted when the display configuration changes.
     *
     * @param config Updated configuration.
     */
    void displayConfigChanged(const fluvel::DisplayConfig& config);

public slots:
    /**
     * @brief Shows or hides the settings panel.
     *
     * @param open True to show the panel, false to hide it.
     */
    void setPanelVisible(bool open);

private:
    /**
     * @brief Animates panel visibility changes.
     */
    void animate(bool open);

    /**
     * @brief Sets brightness, contrast and auto-contrast, and emits the new configuration once.
     */
    void setAdjustments(int brightness, int contrast, bool autoContrast);

    /**
     * @brief Refreshes the value labels and the reset button from the configuration.
     */
    void updateAdjustmentControls();

    QPointer<QPropertyAnimation> animation_;

    QGroupBox* backgroundGroupBox_{nullptr};
    QRadioButton* sourceRadioButton_{nullptr};
    QRadioButton* preprocessedRadioButton_{nullptr};

    ColorSelectorWidget* outerContourColorSelector_{nullptr};
    ColorSelectorWidget* innerContourColorSelector_{nullptr};

    QCheckBox* mirrorModeCheckBox_{nullptr};
    QCheckBox* smoothDisplayCheckBox_{nullptr};

    QSlider* brightnessSlider_{nullptr};
    QSlider* contrastSlider_{nullptr};
    QLabel* brightnessValueLabel_{nullptr};
    QLabel* contrastValueLabel_{nullptr};
    QCheckBox* autoContrastCheckBox_{nullptr};
    QPushButton* resetAdjustmentsButton_{nullptr};

    QCheckBox* overlayCheckBox_{nullptr};
    QCheckBox* miniMapCheckBox_{nullptr};

    DisplayConfig config_{};
    bool panelVisible_{true};
};

} // namespace fluvel
