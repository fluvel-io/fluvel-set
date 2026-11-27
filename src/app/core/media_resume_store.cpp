// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "media_resume_store.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

namespace fluvel
{

namespace
{

constexpr auto kPositionKey = "position_ms";
constexpr auto kDurationKey = "duration_ms";
constexpr auto kLastSeenKey = "last_seen";

/// Drops the oldest entries so that at most kMaxEntries remain.
void prune(QSettings& settings)
{
    const QStringList groups = settings.childGroups();

    if (groups.size() <= MediaResumeStore::kMaxEntries)
        return;

    std::vector<std::pair<qint64, QString>> entries;
    entries.reserve(static_cast<size_t>(groups.size()));

    for (const QString& group : groups)
        entries.emplace_back(settings.value(group + "/" + kLastSeenKey, 0).toLongLong(), group);

    std::ranges::sort(entries);

    const size_t excess = entries.size() - MediaResumeStore::kMaxEntries;

    for (size_t i = 0; i < excess; ++i)
        settings.remove(entries[i].second);
}

} // namespace

MediaResumeStore::MediaResumeStore(QString filePath)
    : filePath_(std::move(filePath))
{
}

QString MediaResumeStore::defaultFilePath()
{
    // Same directory as the other settings files (see ApplicationSettings).
    const QSettings base;

    return QFileInfo(base.fileName()).dir().absolutePath() + "/media_positions.ini";
}

bool MediaResumeStore::isWorthResuming(qint64 positionMs, qint64 durationMs)
{
    return durationMs > 0 && positionMs >= kMinResumePositionMs &&
           positionMs <= durationMs - kEndMarginMs;
}

QString MediaResumeStore::keyFor(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty())
        return {};

    QByteArray identity;

    if (url.isLocalFile())
    {
        const QFileInfo info(url.toLocalFile());

        if (!info.exists())
            return {};

        identity = info.canonicalFilePath().toUtf8() + '|' + QByteArray::number(info.size()) + '|' +
                   QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    }
    else
    {
        // Credentials are not part of the identity (and must not be hashed
        // into a file anyone could compare against).
        identity = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemoveFragment).toEncoded();
    }

    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha1).toHex());
}

std::optional<qint64> MediaResumeStore::positionFor(const QUrl& url, qint64 durationMs) const
{
    const QString key = keyFor(url);

    if (key.isEmpty())
        return std::nullopt;

    const QSettings settings(filePath_, QSettings::IniFormat);

    if (!settings.contains(key + "/" + kPositionKey))
        return std::nullopt;

    const qint64 position = settings.value(key + "/" + kPositionKey).toLongLong();
    const qint64 storedDuration = settings.value(key + "/" + kDurationKey).toLongLong();

    if (std::abs(storedDuration - durationMs) > kDurationToleranceMs)
        return std::nullopt;

    if (!isWorthResuming(position, durationMs))
        return std::nullopt;

    return std::max<qint64>(0, position - kRewindMs);
}

void MediaResumeStore::save(const QUrl& url, qint64 positionMs, qint64 durationMs) const
{
    const QString key = keyFor(url);

    if (key.isEmpty())
        return;

    QSettings settings(filePath_, QSettings::IniFormat);

    if (!isWorthResuming(positionMs, durationMs))
    {
        settings.remove(key);
        return;
    }

    settings.setValue(key + "/" + kPositionKey, positionMs);
    settings.setValue(key + "/" + kDurationKey, durationMs);
    settings.setValue(key + "/" + kLastSeenKey, QDateTime::currentSecsSinceEpoch());

    prune(settings);
}

void MediaResumeStore::forget(const QUrl& url) const
{
    const QString key = keyFor(url);

    if (key.isEmpty())
        return;

    QSettings settings(filePath_, QSettings::IniFormat);
    settings.remove(key);
}

} // namespace fluvel
