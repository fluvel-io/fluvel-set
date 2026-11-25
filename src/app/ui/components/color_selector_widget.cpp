// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "color_selector_widget.hpp"

#include "animated_push_button.hpp"
#include "icon_loader.hpp"

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>

namespace fluvel
{

ColorSelectorWidget::ColorSelectorWidget(QWidget* parent, QColor initialColor)
    : QWidget(parent)
{
    colorCb_ = new QComboBox;

    addColorItem(Qt::red, tr("Red"));
    addColorItem(Qt::green, tr("Green"));
    addColorItem(Qt::blue, tr("Blue"));
    addColorItem(Qt::cyan, tr("Cyan"));
    addColorItem(Qt::magenta, tr("Magenta"));
    addColorItem(Qt::yellow, tr("Yellow"));
    addColorItem(Qt::black, tr("Black"));
    addColorItem(Qt::white, tr("White"));
    addColorItem(Qt::transparent, tr("Custom"));

    QIcon pickerIcon =
        il::loadIcon("color-picker-symbolic", ":/icons/actions/color-picker-symbolic.svg");

    customPb_ = new AnimatedPushButton;
    customPb_->setIcon(pickerIcon);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(colorCb_);
    layout->addWidget(customPb_);

    setSelectedColor(initialColor);

    connect(colorCb_, &QComboBox::currentIndexChanged, this, &ColorSelectorWidget::onIndexChanged);

    connect(customPb_, &QPushButton::clicked, this, &ColorSelectorWidget::onCustomClicked);
}

void ColorSelectorWidget::addColorItem(const QColor& color, const QString& name)
{
    colorCb_->addItem(il::createSquare(color), name, color);
}

QColor ColorSelectorWidget::color() const
{
    return colorCb_->currentData().value<QColor>();
}

void ColorSelectorWidget::onIndexChanged()
{
    emit colorSelected(color());
}

void ColorSelectorWidget::onCustomClicked()
{
    QColor chosen = QColorDialog::getColor(color(), this, tr("Custom color selection"));

    if (!chosen.isValid())
        return;

    const int customIndex = colorCb_->findText(tr("Custom"));
    if (customIndex < 0)
        return;

    QSignalBlocker blocker(colorCb_);

    colorCb_->setItemIcon(customIndex, il::createSquare(chosen));
    colorCb_->setItemData(customIndex, chosen);
    colorCb_->setCurrentIndex(customIndex);

    emit colorSelected(chosen);
}

void ColorSelectorWidget::setSelectedColor(const QColor& color)
{
    QSignalBlocker blocker(colorCb_);

    // chercher si la couleur existe déjà
    for (int i = 0; i < colorCb_->count(); ++i)
    {
        const QVariant v = colorCb_->itemData(i);
        if (v.canConvert<QColor>() && v.value<QColor>() == color)
        {
            colorCb_->setCurrentIndex(i);

            emit colorSelected(color);
            return;
        }
    }

    // sinon → custom
    const int customIndex = colorCb_->findText(tr("Custom"));
    if (customIndex >= 0)
    {
        colorCb_->setItemIcon(customIndex, il::createSquare(color));
        colorCb_->setItemData(customIndex, color);
        colorCb_->setCurrentIndex(customIndex);

        emit colorSelected(color);
    }
}

} // namespace fluvel
