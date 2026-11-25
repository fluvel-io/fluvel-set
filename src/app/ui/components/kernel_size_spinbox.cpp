// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "kernel_size_spinbox.hpp"

namespace fluvel
{

KernelSizeSpinBox::KernelSizeSpinBox(QWidget* parent)
    : QSpinBox(parent)
{
    setSuffix(" × ");

    connect(this, QOverload<const QString&>::of(&QSpinBox::textChanged), this,
            &KernelSizeSpinBox::setSuffix, Qt::UniqueConnection);
}

QValidator::State KernelSizeSpinBox::validate(QString& text, int& pos) const
{
    QString textWithoutSuffix(text);
    textWithoutSuffix.chop(suffix().size());

    qsizetype size = textWithoutSuffix.size();
    bool isSuccessed;
    int value = textWithoutSuffix.toInt(&isSuccessed);

    QValidator::State result;

    // One branch per case of the validation table, kept separate for readability.
    // NOLINTBEGIN(bugprone-branch-clone)
    if (textWithoutSuffix.isEmpty())
    {
        result = QValidator::Intermediate;
    }
    else if (!isSuccessed)
    {
        result = QValidator::Invalid;
    }
    else if (value > maximum())
    {
        result = QValidator::Invalid;
    }
    else if (value < minimum())
    {
        result = QValidator::Intermediate;
    }
    else if (value % 2 == 1)
    {
        result = QValidator::Acceptable;
    }
    else if (pos == size)
    {
        result = QValidator::Intermediate;
    }
    else
    {
        result = QValidator::Invalid;
    }
    // NOLINTEND(bugprone-branch-clone)

    if (hasFocus())
    {
        if (result == QValidator::Acceptable)
        {
            const_cast<KernelSizeSpinBox*>(this)->setStyleSheet("color:green");
        }
        else if (result == QValidator::Intermediate)
        {
            const_cast<KernelSizeSpinBox*>(this)->setStyleSheet("color:orange");
        }
        else if (result == QValidator::Invalid)
        {
            if (previousResult_ == QValidator::Acceptable)
            {
                const_cast<KernelSizeSpinBox*>(this)->setStyleSheet("color:green");
            }
            else
            {
                const_cast<KernelSizeSpinBox*>(this)->setStyleSheet("color:orange");
            }
        }
    }
    else
    {
        const_cast<KernelSizeSpinBox*>(this)->setStyleSheet("");
    }

    if (result != QValidator::Invalid)
    {
        const_cast<KernelSizeSpinBox*>(this)->previousResult_ = result;
    }

    return result;
}

void KernelSizeSpinBox::setSuffix(const QString& text)
{
    QString textWithoutSuffix(text);
    textWithoutSuffix.chop(suffix().size());

    textWithoutSuffix = " × " + textWithoutSuffix;

    QSpinBox::setSuffix(textWithoutSuffix);
}

void KernelSizeSpinBox::focusInEvent(QFocusEvent* event)
{
    setStyleSheet("color:green");
    QAbstractSpinBox::focusInEvent(event);
}

} // namespace fluvel
