// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "volume_loader.hpp"

#include "file_utils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>

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
              [](const QString& a, const QString& b)
              {
                  return naturalCompare(a, b) < 0;
              });

    return files;
}

QStringList imageFilesInFolder(const QString& folder)
{
    const QDir dir(folder);

    const QStringList patterns =
        file_utils::supportedImageExtensions().split(' ', Qt::SkipEmptyParts);

    QStringList files;

    for (const QFileInfo& info : dir.entryInfoList(patterns, QDir::Files | QDir::Readable))
        files << info.absoluteFilePath();

    return sortedNaturally(files);
}

std::optional<RawParameters> guessRawParameters(const QString& fileName)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)x(\\d+)x(\\d+)"),
                                       QRegularExpression::CaseInsensitiveOption);

    const auto match = re.match(QFileInfo(fileName).fileName());

    if (!match.hasMatch())
        return std::nullopt;

    bool okW = false;
    bool okH = false;
    bool okD = false;

    const int w = match.captured(1).toInt(&okW);
    const int h = match.captured(2).toInt(&okH);
    const int d = match.captured(3).toInt(&okD);

    if (!okW || !okH || !okD || w <= 0 || h <= 0 || d <= 0)
        return std::nullopt;

    RawParameters p;
    p.width = w;
    p.height = h;
    p.depth = d;
    return p;
}

bool nameSuggestsOtherVoxelType(const QString& fileName)
{
    static const QRegularExpression re(QStringLiteral("u?int(16|32|64)|float|double"),
                                       QRegularExpression::CaseInsensitiveOption);

    return re.match(QFileInfo(fileName).fileName()).hasMatch();
}

Result loadRaw(const QString& path, const RawParameters& p, const ProgressCallback& progress)
{
    if (p.width <= 0 || p.height <= 0 || p.depth <= 0 || p.headerBytes < 0)
        return failure(QObject::tr("The sizes of the volume must be strictly positive."));

    const qint64 sliceBytes = static_cast<qint64>(p.width) * p.height;
    const qint64 voxels = sliceBytes * p.depth;

    if (voxels > kMaxVoxels)
        return failure(QObject::tr("The volume is too large (%1 x %2 x %3 voxels).")
                           .arg(p.width)
                           .arg(p.height)
                           .arg(p.depth));

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return failure(QObject::tr("Cannot open %1: %2").arg(path, file.errorString()));

    const qint64 actual = file.size();
    const qint64 expected = p.expectedFileSize();

    if (actual != expected)
    {
        QString message = QObject::tr("The file has %1 bytes but %2 are expected for %3 x %4 x %5 "
                                      "voxels of 8 bits.")
                              .arg(actual)
                              .arg(expected)
                              .arg(p.width)
                              .arg(p.height)
                              .arg(p.depth);

        const qint64 payload = actual - p.headerBytes;

        if (payload > 0 && voxels > 0 && payload % voxels == 0 && payload / voxels > 1)
            message += QLatin1Char('\n') +
                       QObject::tr("The file probably holds voxels of %1 bits: only 8 bits are "
                                   "supported for now.")
                           .arg(8 * (payload / voxels));

        return failure(message);
    }

    if (!file.seek(p.headerBytes))
        return failure(QObject::tr("Cannot read %1: %2").arg(path, file.errorString()));

    auto volume = std::make_shared<fluvel_ip::VolumeOwner>(p.width, p.height, p.depth);

    for (int z = 0; z < p.depth; ++z)
    {
        char* dst = reinterpret_cast<char*>(volume->sliceData(z));

        if (file.read(dst, sliceBytes) != sliceBytes)
            return failure(QObject::tr("Cannot read %1: %2").arg(path, file.errorString()));

        if (progress && !progress(z + 1, p.depth))
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
