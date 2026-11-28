// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include "volume_loader.hpp"

#include <QDialog>
#include <QString>

#include <optional>

class QDialogButtonBox;
class QLabel;
class QSpinBox;
class QWidget;

namespace fluvel
{

/**
 * @brief Dialog asking the sizes of a raw volume file.
 *
 * A raw file contains only voxels: its width, height and depth cannot be read from it. The
 * fields are pre-filled from the file name when it contains them (for example
 * "tooth_103x94x161_uint8.raw"). The dialog compares the size announced by the fields with the
 * size of the file and enables OK only when they match, which avoids loading a skewed volume
 * because of a wrong dimension.
 */
class RawVolumeDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit RawVolumeDialog(const QString& filePath, QWidget* parent = nullptr);

    /// Parameters currently entered in the dialog.
    volume_loader::RawParameters parameters() const;

    /// True when the entered parameters match the file size (OK is enabled).
    bool matchesFileSize() const;

    /**
     * @brief Shows the dialog.
     *
     * @return The parameters if the user accepted, std::nullopt if the dialog was cancelled.
     */
    static std::optional<volume_loader::RawParameters> ask(const QString& filePath,
                                                           QWidget* parent = nullptr);

private:
    void updateStatus();

    qint64 fileSize_{0};
    bool otherTypeSuspected_{false};

    QSpinBox* widthSpin_{nullptr};
    QSpinBox* heightSpin_{nullptr};
    QSpinBox* depthSpin_{nullptr};
    QSpinBox* headerSpin_{nullptr};
    QLabel* statusLabel_{nullptr};
    QDialogButtonBox* buttons_{nullptr};
};

} // namespace fluvel
