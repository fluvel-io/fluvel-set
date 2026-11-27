// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_loader.hpp"

#include "file_utils.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>

#include <algorithm>
#include <cstring>

namespace fluvel::volume_loader
{

namespace
{

Result failure(const QString& message)
{
    Result r;
    r.error = message;
    return r;
}

/// Reads the page @p page of the file as a grayscale image (null image on error).
QImage readGray(QImageReader& reader, QString& error)
{
    QImage image = reader.read();

    if (image.isNull())
    {
        error = reader.errorString();
        return {};
    }

    if (image.format() != QImage::Format_Grayscale8)
        image = image.convertToFormat(QImage::Format_Grayscale8);

    return image;
}

/// Copies a grayscale image into a tightly packed slice.
void copySlice(const QImage& image, std::uint8_t* dst)
{
    const int w = image.width();

    for (int y = 0; y < image.height(); ++y)
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w),
                    image.constScanLine(y), static_cast<std::size_t>(w));
}

/**
 * @brief Compares two strings, comparing runs of digits by value.
 *
 * Implemented here instead of QCollator because the numeric mode of QCollator is not
 * available on every platform. Letters are compared without case.
 *
 * @return <0, 0 or >0.
 */
int naturalCompare(const QString& a, const QString& b)
{
    qsizetype i = 0;
    qsizetype j = 0;

    while (i < a.size() && j < b.size())
    {
        if (a[i].isDigit() && b[j].isDigit())
        {
            // Skip leading zeros, then the longer run of digits is the larger number.
            qsizetype ei = i;
            qsizetype ej = j;

            while (ei < a.size() && a[ei].isDigit())
                ++ei;
            while (ej < b.size() && b[ej].isDigit())
                ++ej;

            qsizetype si = i;
            qsizetype sj = j;

            while (si < ei - 1 && a[si] == QLatin1Char('0'))
                ++si;
            while (sj < ej - 1 && b[sj] == QLatin1Char('0'))
                ++sj;

            const qsizetype lenA = ei - si;
            const qsizetype lenB = ej - sj;

            if (lenA != lenB)
                return lenA < lenB ? -1 : 1;

            const int cmp = QStringView(a).mid(si, lenA).compare(QStringView(b).mid(sj, lenB));

            if (cmp != 0)
                return cmp;

            i = ei;
            j = ej;
        }
        else
        {
            const QChar ca = a[i].toCaseFolded();
            const QChar cb = b[j].toCaseFolded();

            if (ca != cb)
                return ca < cb ? -1 : 1;

            ++i;
            ++j;
        }
    }

    // The shorter string comes first.
    return static_cast<int>((a.size() - i) - (b.size() - j));
}

} // namespace

QStringList sortedNaturally(QStringList files)
{
    std::sort(files.begin(), files.end(),
              [](const QString& a, const QString& b) { return naturalCompare(a, b) < 0; });

    return files;
}

QStringList imageFilesInFolder(const QString& folder)
{
    const QDir dir(folder);

    const QStringList patterns = file_utils::supportedImageExtensions().split(' ', Qt::SkipEmptyParts);

    QStringList files;

    for (const QFileInfo& info : dir.entryInfoList(patterns, QDir::Files | QDir::Readable))
        files << info.absoluteFilePath();

    return sortedNaturally(files);
}

Result loadImageSeries(const QStringList& files, const ProgressCallback& progress)
{
    if (files.isEmpty())
        return failure(QObject::tr("No image to load."));

    // A single multi-page file gives one slice per page.
    const bool multiPage = files.size() == 1;

    int total = files.size();

    if (multiPage)
    {
        QImageReader probe(files.first());
        total = std::max(probe.imageCount(), 1);
    }

    QSize size;
    std::shared_ptr<fluvel_ip::VolumeOwner> volume;

    // For a multi-page file the same reader is kept: each read() gives the next page.
    QImageReader pageReader(files.first());

    for (int i = 0; i < total; ++i)
    {
        QImageReader fileReader(multiPage ? QString() : files[i]);
        QImageReader& reader = multiPage ? pageReader : fileReader;

        QString error;
        const QImage image = readGray(reader, error);

        if (image.isNull())
            return failure(QObject::tr("Cannot read %1: %2").arg(files[multiPage ? 0 : i], error));

        if (!volume)
        {
            size = image.size();

            const qint64 voxels = static_cast<qint64>(size.width()) * size.height() * total;

            if (voxels > kMaxVoxels)
                return failure(QObject::tr("The volume is too large (%1 x %2 x %3 voxels).")
                                   .arg(size.width())
                                   .arg(size.height())
                                   .arg(total));

            volume = std::make_shared<fluvel_ip::VolumeOwner>(size.width(), size.height(), total);
        }
        else if (image.size() != size)
        {
            return failure(QObject::tr("%1 is %2 x %3 but the first image is %4 x %5. "
                                       "All the images must have the same size.")
                               .arg(QFileInfo(files[multiPage ? 0 : i]).fileName())
                               .arg(image.width())
                               .arg(image.height())
                               .arg(size.width())
                               .arg(size.height()));
        }

        copySlice(image, volume->sliceData(i));

        if (progress && !progress(i + 1, total))
        {
            Result r;
            r.cancelled = true;
            return r;
        }
    }

    Result r;
    r.volume = std::move(volume);
    return r;
}

} // namespace fluvel::volume_loader
