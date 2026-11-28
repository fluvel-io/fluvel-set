// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "raw_volume_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

namespace fluvel
{

namespace
{

QSpinBox* makeSizeSpin(QWidget* parent)
{
    auto* spin = new QSpinBox(parent);
    spin->setRange(1, 65535);
    spin->setGroupSeparatorShown(false);
    return spin;
}

} // namespace

RawVolumeDialog::RawVolumeDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Open raw volume"));

    const QFileInfo info(filePath);
    fileSize_ = info.size();
    unsupportedTypeSuspected_ = volume_loader::nameSuggestsUnsupportedVoxelType(info.fileName());

    auto* nameLabel = new QLabel(info.fileName(), this);
    nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    widthSpin_ = makeSizeSpin(this);
    heightSpin_ = makeSizeSpin(this);
    depthSpin_ = makeSizeSpin(this);

    headerSpin_ = new QSpinBox(this);
    headerSpin_->setRange(0, std::numeric_limits<int>::max());
    headerSpin_->setSuffix(tr(" bytes"));
    headerSpin_->setToolTip(tr("Number of bytes to skip at the beginning of the file."));

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(tr("8-bit unsigned"), static_cast<int>(VoxelType::UInt8));
    typeCombo_->addItem(tr("16-bit unsigned"), static_cast<int>(VoxelType::UInt16));
    typeCombo_->addItem(tr("16-bit signed"), static_cast<int>(VoxelType::Int16));
    typeCombo_->addItem(tr("32-bit float"), static_cast<int>(VoxelType::Float32));

    byteOrderCombo_ = new QComboBox(this);
    byteOrderCombo_->addItem(tr("Little endian (most files)"), false);
    byteOrderCombo_->addItem(tr("Big endian"), true);
    byteOrderCombo_->setToolTip(tr("Order of the bytes of the voxels of 16 bits or more."));

    auto* form = new QFormLayout;
    form->addRow(tr("File:"), nameLabel);
    form->addRow(tr("Width:"), widthSpin_);
    form->addRow(tr("Height:"), heightSpin_);
    form->addRow(tr("Depth (number of slices):"), depthSpin_);
    form->addRow(tr("Header:"), headerSpin_);
    form->addRow(tr("Data type:"), typeCombo_);
    form->addRow(tr("Byte order:"), byteOrderCombo_);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setMinimumHeight(statusLabel_->fontMetrics().lineSpacing() * 3);
    statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(statusLabel_);
    layout->addWidget(buttons_);

    // Pre-fill from the file name ("name_103x94x161_uint8.raw").
    if (const auto guess = volume_loader::guessRawParameters(filePath); guess.has_value())
    {
        widthSpin_->setValue(guess->width);
        heightSpin_->setValue(guess->height);
        depthSpin_->setValue(guess->depth);

        if (const int index = typeCombo_->findData(static_cast<int>(guess->type)); index >= 0)
            typeCombo_->setCurrentIndex(index);
    }

    for (QSpinBox* spin : {widthSpin_, heightSpin_, depthSpin_, headerSpin_})
        connect(spin, &QSpinBox::valueChanged, this, &RawVolumeDialog::updateStatus);

    connect(typeCombo_, &QComboBox::currentIndexChanged, this, &RawVolumeDialog::updateStatus);
    connect(typeCombo_, &QComboBox::currentIndexChanged, this,
            &RawVolumeDialog::updateByteOrderEnabled);

    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateByteOrderEnabled();
    updateStatus();
}

volume_loader::RawParameters RawVolumeDialog::parameters() const
{
    volume_loader::RawParameters p;
    p.width = widthSpin_->value();
    p.height = heightSpin_->value();
    p.depth = depthSpin_->value();
    p.headerBytes = headerSpin_->value();
    p.type = static_cast<VoxelType>(typeCombo_->currentData().toInt());
    p.bigEndian = byteOrderCombo_->currentData().toBool();
    return p;
}

bool RawVolumeDialog::matchesFileSize() const
{
    return parameters().expectedFileSize() == fileSize_;
}

void RawVolumeDialog::updateStatus()
{
    const qint64 expected = parameters().expectedFileSize();
    const bool matches = expected == fileSize_;

    QString text;

    if (matches)
    {
        text = tr("The sizes match the file (%1 bytes).").arg(fileSize_);
    }
    else
    {
        text = tr("The sizes do not match the file: %1 bytes expected, %2 bytes in the file.")
                   .arg(expected)
                   .arg(fileSize_);
    }

    if (unsupportedTypeSuspected_)
        text += QLatin1Char('\n') + tr("The file name suggests a voxel type that is not "
                                       "supported.");

    statusLabel_->setText(text);

    if (auto* ok = buttons_->button(QDialogButtonBox::Ok))
        ok->setEnabled(matches);
}

void RawVolumeDialog::updateByteOrderEnabled()
{
    // The byte order only matters for the voxels of 2 bytes or more.
    const auto type = static_cast<VoxelType>(typeCombo_->currentData().toInt());
    byteOrderCombo_->setEnabled(bytesPerVoxel(type) > 1);
}

std::optional<volume_loader::RawParameters> RawVolumeDialog::ask(const QString& filePath,
                                                                 QWidget* parent)
{
    RawVolumeDialog dialog(filePath, parent);

    if (dialog.exec() != QDialog::Accepted)
        return std::nullopt;

    return dialog.parameters();
}

} // namespace fluvel
