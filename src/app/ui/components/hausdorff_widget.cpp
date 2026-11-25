// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "hausdorff_widget.hpp"

#include "hausdorff_window.hpp"
#include "animated_push_button.hpp"
#include "color_adapters.hpp"
#include "color_picker_behavior.hpp"
#include "drag_drop_behavior.hpp"
#include "file_utils.hpp"
#include "icon_loader.hpp"
#include "image_viewer_widget.hpp"
#include "interaction_set.hpp"
#include "pan_behavior.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cassert>

namespace fluvel
{

namespace
{
/// Side of the mini-maps: smaller than the default, since two views share the window.
constexpr int kMiniMapSize{100};

/// Presentation of the viewers of the Hausdorff window.
ViewConfig hausdorffViewConfig()
{
    ViewConfig view;
    view.algorithmOverlayEnabled = false;
    view.miniMapEnabled = true;
    view.miniMapSize = kMiniMapSize;
    return view;
}
} // namespace

int HausdorffWidget::countThis_ = 0;

HausdorffWidget::HausdorffWidget(QWidget* parent)
    : QWidget(parent)
{
    ++countThis_;         // static variable to count the instances
    idThis_ = countThis_; // in order to know if *this is the first or the second widget of
                         // hausdorff_window

    QSettings settings;

    /////////////////////////////////////////////////////////////////////////////////

    nameLabel_ = new QLabel(this);
    nameLabel_->setText(tr("Title - Size"));
    nameLabel_->setAlignment(Qt::AlignCenter);

    ///////////////////////////////////////

    imageViewer_ = new ImageViewerWidget(hausdorffViewConfig(), this);
    auto interaction = std::make_unique<InteractionSet>();
    // interaction->addBehavior(std::make_unique<AutoFitBehavior>());
    // interaction->addBehavior(std::make_unique<FullscreenBehavior>());
    interaction->addBehavior(std::make_unique<PanBehavior>());
    interaction->addBehavior(std::make_unique<ColorPickerBehavior>());
    interaction->addBehavior(std::make_unique<DragDropBehavior>(
        DragDropContent::Images, tr("Drop an image here\n\nor click Open...")));
    imageViewer_->setInteraction(interaction.release());

    imageViewer_->setListener(this);

    ///////////////////////////////////////

    QIcon imageIcon = il::loadIcon("image-x-generic-symbolic", ":/icons/file/image-symbolic.svg");

    openButton_ = new AnimatedPushButton(tr("Open..."));
    openButton_->setIcon(imageIcon);

    QVBoxLayout* imgLayout = new QVBoxLayout;
    imgLayout->addWidget(nameLabel_);
    imgLayout->addWidget(imageViewer_);
    imgLayout->addWidget(openButton_);
    QGroupBox* imgGroup = new QGroupBox(tr("Image") + " " + QString::number(idThis_));
    imgGroup->setLayout(imgLayout);

    //////////////////////////////////////////////////////////////////////////////////

    selectedColor_.red = static_cast<unsigned char>(
        settings.value("Hausdorff/R" + QString::number(idThis_), 128).toInt());
    selectedColor_.green = static_cast<unsigned char>(
        settings.value("Hausdorff/G" + QString::number(idThis_), 0).toInt());
    selectedColor_.blue = static_cast<unsigned char>(
        settings.value("Hausdorff/B" + QString::number(idThis_), 255).toInt());

    QColor initialColor = toQColor(selectedColor_);
    colorSelector_ = new ColorSelectorWidget(this, initialColor);

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("Color: "), colorSelector_);

    QGroupBox* colorGroup = new QGroupBox(tr("Color") + " " + QString::number(idThis_));
    colorGroup->setLayout(form);

    ///////////////////////////////////////

    noiseSp_ = new QSpinBox;
    noiseSp_->setSingleStep(1);
    noiseSp_->setMinimum(0);
    noiseSp_->setMaximum(100);
    noiseSp_->setSuffix(" %");
    noiseSp_->setValue(0);

    noiseGroup_ = new QGroupBox(tr("Noise"));
    noiseGroup_->setCheckable(true);
    noiseGroup_->setChecked(false);

    QFormLayout* noiseLayout = new QFormLayout;
    noiseLayout->addRow(tr("Amount:"), noiseSp_);

    noiseGroup_->setLayout(noiseLayout);

    textListLength_ = new QLabel(this);
    textListLength_->setAlignment(Qt::AlignCenter);
    if (idThis_ == 1)
    {
        textListLength_->setText("<font color=red>" +
                                 tr("Shape 1 size: %1 pts").arg(shape_.points().size()));
    }
    else if (idThis_ == 2)
    {
        textListLength_->setText("<font color=red>" +
                                 tr("Shape 2 size: %1 pts").arg(shape_.points().size()));
    }

    QVBoxLayout* thisLayout = new QVBoxLayout;
    thisLayout->addWidget(imgGroup);
    thisLayout->addWidget(colorGroup);
    thisLayout->addWidget(noiseGroup_);
    thisLayout->addWidget(textListLength_);

    setLayout(thisLayout);

    lastDirectoryUsed_ = settings.value("history/last_directory", QDir().homePath()).toString();

    connect(openButton_, &QPushButton::clicked, this, &HausdorffWidget::openFilename);

    // When the user drags and drops an image into the viewer.
    connect(imageViewer_, &ImageViewerWidget::imageDropped, this,
            &HausdorffWidget::openDroppedImage);

    connect(colorSelector_, &ColorSelectorWidget::colorSelected, this,
            &HausdorffWidget::onColorSelected);

    auto* hausdorffWindow = qobject_cast<HausdorffWindow*>(parentWidget());
    Q_ASSERT(hausdorffWindow);

    connect(this, &HausdorffWidget::listChanged, hausdorffWindow, &HausdorffWindow::checkLists);

    connect(noiseGroup_, &QGroupBox::toggled, this,
            [this](bool)
            {
                refreshWithNoise(noiseSp_->value());
            });

    connect(noiseSp_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &HausdorffWidget::refreshWithNoise);

    refresh();
}

void HausdorffWidget::onColorSelected(const QColor& c)
{
    selectedColor_ = toRgbUc(c);
    refresh();
}

void HausdorffWidget::openFilename()
{
    const QString fileName =
        QFileDialog::getOpenFileName(this, tr("Open Image") + " " + QString::number(idThis_),
                                     lastDirectoryUsed_, file_utils::buildImageFilter());

    if (fileName.isEmpty())
        return;

    absoluteName_ = fileName;

    openImage();
}

void HausdorffWidget::openDroppedImage(const QString& fileName)
{
    if (fileName.isEmpty())
        return;

    absoluteName_ = fileName;

    openImage();
}

void HausdorffWidget::openImage()
{
    assert(!absoluteName_.isEmpty());

    QImageReader reader(absoluteName_);

    image_ = reader.read();

    if (image_.isNull())
    {
        QMessageBox::information(this, tr("Cannot Open Image"),
                                 tr("Failed to load image: %1").arg(reader.errorString()));

        return;
    }

    refresh();

    QFileInfo fi(absoluteName_);
    QString name = fi.fileName();

    QString stringListsText;
    stringListsText = QString::number(image_.width()) + "×" + QString::number(image_.height());
    nameLabel_->setText(name + " - " + stringListsText);
}

void HausdorffWidget::refresh()
{
    refreshWithNoise(noiseSp_->value());
}

void HausdorffWidget::createList()
{
    shape_.clear();

    QRgb pix;

    for (int y = 0; y < noiseImage_.height(); ++y)
    {
        for (int x = 0; x < noiseImage_.width(); ++x)
        {
            pix = noiseImage_.pixel(x, y);

            if (toRgbUc(pix) == selectedColor_)
                shape_.pushBack(x, y);
        }
    }

    shape_.calculateCentroid();

    QString sizeStr = QString::number(shape_.points().size());

    QString colorStr;
    if (shape_.points().empty())
        colorStr = "<font color=red>";
    else
        colorStr = "<font color=green>";

    if (idThis_ == 1)
    {
        textListLength_->setText(colorStr + tr("Shape 1 size: %1 pts").arg(sizeStr));
    }
    else if (idThis_ == 2)
    {
        textListLength_->setText(colorStr + tr("Shape 2 size: %1 pts").arg(sizeStr));
    }

    emit listChanged();
}

void HausdorffWidget::refreshWithNoise(int noisePercent)
{
    if (image_.isNull())
        return;

    noiseImage_ = image_;

    // Early return when noise is disabled.
    if (!noiseGroup_->isChecked() || noisePercent == 0)
    {
        imageViewer_->setImage(noiseImage_);
        createList();
        return;
    }

    std::bernoulli_distribution probaDistri{float(noisePercent) / 100.f};

    QRgb rgbColor =
        QColor(int(selectedColor_.red), int(selectedColor_.green), int(selectedColor_.blue)).rgb();

    for (int y = 0; y < noiseImage_.height(); ++y)
    {
        for (int x = 0; x < noiseImage_.width(); ++x)
        {
            if (probaDistri(rng_))
                noiseImage_.setPixel(x, y, rgbColor);
        }
    }

    imageViewer_->setImage(noiseImage_);
    createList();
}

void HausdorffWidget::onColorPicked(const QColor& color, const QPoint&)
{
    if (image_.isNull())
        return;

    colorSelector_->setSelectedColor(color);
}

void HausdorffWidget::saveSettings() const
{
    QSettings settings;

    settings.setValue("history/last_directory", lastDirectoryUsed_);

    // settings.setValue("Hausdorff/combo" + QString::number(idThis_), colorList_->currentIndex());

    settings.setValue("Hausdorff/R" + QString::number(idThis_), int(selectedColor_.red));
    settings.setValue("Hausdorff/G" + QString::number(idThis_), int(selectedColor_.green));
    settings.setValue("Hausdorff/B" + QString::number(idThis_), int(selectedColor_.blue));
}

} // namespace fluvel
