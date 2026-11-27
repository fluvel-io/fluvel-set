// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#pragma once

#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <optional>

namespace fluvel
{

/**
 * @brief Remembers the last playback position of seekable media.
 *
 * Positions are kept in a small INI file stored next to the other settings
 * files. A media is identified by a hash, so the file holds neither paths nor
 * titles. For a local file the hash covers the canonical path, the size and the
 * modification time, so a modified or moved file is not resumed by mistake.
 *
 * The store is a plain value type: every call opens, updates and closes the
 * file. It is meant to be called from the main thread a few times per minute,
 * not from a hot path.
 */
class MediaResumeStore
{
public:
    /// Positions closer than this to the start are not worth resuming.
    static constexpr qint64 kMinResumePositionMs{5'000};

    /// Positions closer than this to the end are considered as finished.
    static constexpr qint64 kEndMarginMs{5'000};

    /// A resumed playback starts slightly before the saved position.
    static constexpr qint64 kRewindMs{2'000};

    /// Stored durations may differ by this much from the opened media.
    static constexpr qint64 kDurationToleranceMs{1'000};

    /// Oldest entries are dropped beyond this count.
    static constexpr int kMaxEntries{200};

    /**
     * @brief Constructs a store backed by the given INI file.
     *
     * @param filePath Path of the INI file. Defaults to the settings directory.
     */
    explicit MediaResumeStore(QString filePath = defaultFilePath());

    /**
     * @brief Returns the default INI file path, next to the other settings files.
     */
    [[nodiscard]] static QString defaultFilePath();

    /**
     * @brief Tells whether a position is worth saving.
     *
     * @param positionMs Playback position in milliseconds.
     * @param durationMs Media duration in milliseconds.
     */
    [[nodiscard]] static bool isWorthResuming(qint64 positionMs, qint64 durationMs);

    /**
     * @brief Returns the position at which to resume the given media.
     *
     * @param url Media URL.
     * @param durationMs Duration of the media that was just opened.
     *
     * @return The position in milliseconds, or nothing if the media is unknown,
     *         was modified, or was left near its start or its end.
     */
    [[nodiscard]] std::optional<qint64> positionFor(const QUrl& url, qint64 durationMs) const;

    /**
     * @brief Saves the playback position of a media.
     *
     * A position near the start or near the end removes the entry instead,
     * since there is nothing to resume.
     */
    void save(const QUrl& url, qint64 positionMs, qint64 durationMs) const;

    /**
     * @brief Removes the entry of a media, if any.
     */
    void forget(const QUrl& url) const;

private:
    /// Returns the entry key of a media, or an empty string if it has none.
    [[nodiscard]] static QString keyFor(const QUrl& url);

    QString filePath_;
};

} // namespace fluvel
