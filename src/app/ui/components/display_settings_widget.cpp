// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "display_settings_widget.hpp"
#include "color_adapters.hpp"
#include "color_selector_widget.hpp"
#include "jump_slider.hpp"

#include <QColorDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cassert>

namespace fluvel
{

DisplaySettingsWidget::DisplaySettingsWidget(const DisplayConfig& config, QWidget* parent)
    : QWidget(parent)
    , config_(config)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    backgroundGroupBox_ = new QGroupBox(tr("Image:"));
    sourceRadioButton_ = new QRadioButton(tr("Source"));
    preprocessedRadioButton_ = new QRadioButton(tr("Preprocessed"));
    sourceRadioButton_->setChecked(config_.background == BackgroundImage::Source);
    preprocessedRadioButton_->setChecked(config_.background == BackgroundImage::Preprocessed);

    QVBoxLayout* backgroundLayout = new QVBoxLayout;
    backgroundLayout->addWidget(sourceRadioButton_);
    backgroundLayout->addWidget(preprocessedRadioButton_);
    backgroundGroupBox_->setLayout(backgroundLayout);

    outerContourColorSelector_ =
        new ColorSelectorWidget(this, toQColor(config_.contour.outerColor));

    innerContourColorSelector_ =
        new ColorSelectorWidget(this, toQColor(config_.contour.innerColor));

    QVBoxLayout* outerLayout = new QVBoxLayout;
    outerLayout->addWidget(outerContourColorSelector_);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* outerColorGroupBox = new QGroupBox(tr("Outer Contour"));
    outerColorGroupBox->setLayout(outerLayout);
    outerColorGroupBox->setCheckable(true);
    outerColorGroupBox->setChecked(config_.contour.outerVisible);

    outerColorGroupBox->setFlat(true);
    outerColorGroupBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    QVBoxLayout* innerLayout = new QVBoxLayout;
    innerLayout->addWidget(innerContourColorSelector_);
    innerLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* innerColorGroupBox = new QGroupBox(tr("Inner Contour"));
    innerColorGroupBox->setLayout(innerLayout);
    innerColorGroupBox->setCheckable(true);
    innerColorGroupBox->setChecked(config_.contour.innerVisible);

    innerColorGroupBox->setFlat(true);
    innerColorGroupBox->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    mirrorModeCheckBox_ = new QCheckBox(tr("Selfie Mirror"));
    mirrorModeCheckBox_->setChecked(config_.view.mirrorMode);

    smoothDisplayCheckBox_ = new QCheckBox(tr("Smooth Display"));
    smoothDisplayCheckBox_->setChecked(config_.view.smoothDisplay);

    // Brightness and contrast only change what is drawn on screen.
    constexpr int kLimit = ViewConfig::kAdjustmentLimit;

    brightnessSlider_ = new JumpSlider(this, Qt::Horizontal);
    brightnessSlider_->setRange(-kLimit, kLimit);
    brightnessSlider_->setValue(config_.view.brightness);

    contrastSlider_ = new JumpSlider(this, Qt::Horizontal);
    contrastSlider_->setRange(-kLimit, kLimit);
    contrastSlider_->setValue(config_.view.contrast);

    brightnessValueLabel_ = new QLabel;
    contrastValueLabel_ = new QLabel;
    for (QLabel* label : {brightnessValueLabel_, contrastValueLabel_})
    {
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        label->setMinimumWidth(label->fontMetrics().horizontalAdvance(QStringLiteral("+100")));
    }

    autoContrastCheckBox_ = new QCheckBox(tr("Auto contrast"));
    autoContrastCheckBox_->setChecked(config_.view.autoContrast);

    resetAdjustmentsButton_ = new QPushButton(tr("Reset"));

    auto* adjustmentLayout = new QGridLayout;
    adjustmentLayout->addWidget(new QLabel(tr("Brightness")), 0, 0);
    adjustmentLayout->addWidget(brightnessSlider_, 0, 1);
    adjustmentLayout->addWidget(brightnessValueLabel_, 0, 2);
    adjustmentLayout->addWidget(new QLabel(tr("Contrast")), 1, 0);
    adjustmentLayout->addWidget(contrastSlider_, 1, 1);
    adjustmentLayout->addWidget(contrastValueLabel_, 1, 2);
    adjustmentLayout->setColumnMinimumWidth(1, 110);

    auto* autoContrastLayout = new QHBoxLayout;
    autoContrastLayout->addWidget(autoContrastCheckBox_);
    autoContrastLayout->addStretch();
    autoContrastLayout->addWidget(resetAdjustmentsButton_);

    QGroupBox* renderingGroupBox = new QGroupBox(tr("Rendering:"));
    QVBoxLayout* renderingLayout = new QVBoxLayout;
    renderingLayout->addWidget(mirrorModeCheckBox_);
    renderingLayout->addWidget(smoothDisplayCheckBox_);
    renderingLayout->addSpacing(6);
    renderingLayout->addLayout(adjustmentLayout);
    renderingLayout->addLayout(autoContrastLayout);
    renderingGroupBox->setLayout(renderingLayout);

    overlayCheckBox_ = new QCheckBox(tr("Overlay"));
    overlayCheckBox_->setChecked(config_.view.algorithmOverlayEnabled);

    miniMapCheckBox_ = new QCheckBox(tr("Mini-map"));
    miniMapCheckBox_->setChecked(config_.view.miniMapEnabled);

    auto* title = new QLabel(tr("View"));
    title->setStyleSheet("font-weight: bold; font-size: 14px;");

    QVBoxLayout* widgetLayout = new QVBoxLayout;
    widgetLayout->addWidget(title);
    widgetLayout->addSpacing(12);
    widgetLayout->addWidget(backgroundGroupBox_);
    widgetLayout->addWidget(outerColorGroupBox);
    widgetLayout->addWidget(innerColorGroupBox);
    widgetLayout->addWidget(renderingGroupBox);
    widgetLayout->addSpacing(6);
    widgetLayout->addWidget(overlayCheckBox_);
    widgetLayout->addWidget(miniMapCheckBox_);

    widgetLayout->addStretch();

    setLayout(widgetLayout);

    connect(outerColorGroupBox, &QGroupBox::toggled, this,
            [this](bool checked)
            {
                config_.contour.outerVisible = checked;
                emit displayConfigChanged(config_);
            });

    connect(outerContourColorSelector_, &ColorSelectorWidget::colorSelected, this,
            [this](const QColor& color)
            {
                config_.contour.outerColor = toRgbUc(color);
                emit displayConfigChanged(config_);
            });

    connect(innerColorGroupBox, &QGroupBox::toggled, this,
            [this](bool checked)
            {
                config_.contour.innerVisible = checked;
                emit displayConfigChanged(config_);
            });

    connect(innerContourColorSelector_, &ColorSelectorWidget::colorSelected, this,
            [this](const QColor& color)
            {
                config_.contour.innerColor = toRgbUc(color);
                emit displayConfigChanged(config_);
            });

    connect(overlayCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                config_.view.algorithmOverlayEnabled = checked;
                emit displayConfigChanged(config_);
            });

    connect(miniMapCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                config_.view.miniMapEnabled = checked;
                emit displayConfigChanged(config_);
            });

    connect(sourceRadioButton_, &QRadioButton::toggled, this,
            [this](bool checked)
            {
                if (!checked)
                    return;

                config_.background = BackgroundImage::Source;
                emit displayConfigChanged(config_);
            });

    connect(preprocessedRadioButton_, &QRadioButton::toggled, this,
            [this](bool checked)
            {
                if (!checked)
                    return;

                config_.background = BackgroundImage::Preprocessed;
                emit displayConfigChanged(config_);
            });

    connect(mirrorModeCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                config_.view.mirrorMode = checked;
                emit displayConfigChanged(config_);
            });

    connect(smoothDisplayCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked)
            {
                config_.view.smoothDisplay = checked;
                emit displayConfigChanged(config_);
            });

    connect(brightnessSlider_, &QSlider::valueChanged, this,
            [this](int value)
            { setAdjustments(value, config_.view.contrast, config_.view.autoContrast); });

    connect(contrastSlider_, &QSlider::valueChanged, this,
            [this](int value)
            { setAdjustments(config_.view.brightness, value, config_.view.autoContrast); });

    connect(autoContrastCheckBox_, &QCheckBox::toggled, this,
            [this](bool checked)
            { setAdjustments(config_.view.brightness, config_.view.contrast, checked); });

    connect(resetAdjustmentsButton_, &QPushButton::clicked, this,
            [this]() { setAdjustments(0, 0, false); });

    updateAdjustmentControls();
}

void DisplaySettingsWidget::setAdjustments(int brightness, int contrast, bool autoContrast)
{
    config_.view.brightness = brightness;
    config_.view.contrast = contrast;
    config_.view.autoContrast = autoContrast;

    // The controls follow the configuration (needed by the reset button), without
    // triggering their own handlers: the configuration is emitted once, below.
    {
        const QSignalBlocker brightnessBlocker(brightnessSlider_);
        const QSignalBlocker contrastBlocker(contrastSlider_);
        const QSignalBlocker autoBlocker(autoContrastCheckBox_);

        brightnessSlider_->setValue(brightness);
        contrastSlider_->setValue(contrast);
        autoContrastCheckBox_->setChecked(autoContrast);
    }

    updateAdjustmentControls();

    emit displayConfigChanged(config_);
}

void DisplaySettingsWidget::updateAdjustmentControls()
{
    const auto signedText = [](int value)
    { return value > 0 ? QStringLiteral("+%1").arg(value) : QString::number(value); };

    brightnessValueLabel_->setText(signedText(config_.view.brightness));
    contrastValueLabel_->setText(signedText(config_.view.contrast));

    resetAdjustmentsButton_->setEnabled(config_.view.brightness != 0 ||
                                        config_.view.contrast != 0 || config_.view.autoContrast);
}

void DisplaySettingsWidget::updateBackgroundAvailability(bool hasPreprocessing)
{
    backgroundGroupBox_->setEnabled(hasPreprocessing);

    if (!hasPreprocessing)
    {
        sourceRadioButton_->setChecked(true);
        preprocessedRadioButton_->setChecked(false);
    }
}

void DisplaySettingsWidget::setPanelVisible(bool visible)
{
    if (panelVisible_ == visible)
        return;

    panelVisible_ = visible;

    animate(panelVisible_);
}

void DisplaySettingsWidget::setMirrorModeEnabled(bool enabled)
{
    mirrorModeCheckBox_->setChecked(enabled);
}

void DisplaySettingsWidget::setSmoothDisplayEnabled(bool enabled)
{
    smoothDisplayCheckBox_->setChecked(enabled);
}

void DisplaySettingsWidget::setAlgorithmOverlayEnabled(bool enabled)
{
    overlayCheckBox_->setChecked(enabled);
}

void DisplaySettingsWidget::animate(bool open)
{
    if (animation_)
        animation_->stop();

    // Ensure the widget is visible before measuring its preferred width.
    if (open)
        show();

    // Compute the preferred width from the current layout.
    adjustSize();
    const int targetWidth = minimumSizeHint().width();

    const int start = maximumWidth();
    const int end = open ? targetWidth : 0;

    animation_ = new QPropertyAnimation(this, "maximumWidth");

    animation_->setDuration(200);
    animation_->setStartValue(start);
    animation_->setEndValue(end);
    animation_->setEasingCurve(open ? QEasingCurve::OutCubic : QEasingCurve::InCubic);

    connect(animation_, &QPropertyAnimation::finished, this,
            [this, open, anim = animation_]()
            {
                if (animation_ != anim)
                    return;

                if (!open)
                    hide();

                animation_ = nullptr;
            });

    animation_->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace fluvel
