// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

/**
 * @file application_settings_types.hpp
 * @brief Application-level configuration types for Fluvel.
 *
 * This module defines configuration structures used by the application layer
 * (fluvel), including display settings, processing parameters, and session
 * configurations for both image and video workflows.
 *
 * These types act as a bridge between UI controls and the core processing layer.
 */

#pragma once

#include "active_contour_types.hpp"
#include "color.hpp"
#include "contour_types.hpp"
#include "processing_params.hpp"
#include "speed_model_types.hpp"
#include "video_export_settings.hpp"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <string_view>

namespace fluvel
{

/**
 * @brief Application language selection.
 */
enum class Language
{
    System,            ///< Use system locale
    English,           ///< English language
    French,            ///< French language
    Spanish,           ///< Spanish language
    German,            ///< German language
    ChineseSimplified, ///< Simplified Chinese language
    PortugueseBrazil   ///< Brazilian Portuguese language
};

/**
 * @brief Persistent snapshot preferences.
 */
struct SnapshotPreferences
{
    QString directory;
    QString baseName{"image"};
    QByteArray preferredFormat{"png"};
    bool appendTimestamp{true};
};

/**
 * @brief Persistent video recording preferences.
 */
struct VideoRecordingPreferences
{
    static constexpr int kDefaultRetentionTimeMinutes{30};
    static constexpr int kDefaultSegmentCount{10};

    QString directory;
    QString baseName{"video"};

    VideoCodec preferredCodec{VideoCodec::FFV1};

    bool appendTimestamp{true};

    RecordingMode recordingMode{RecordingMode::SingleFile};

    bool convertToStandardMp4{false};

    int retentionTimeMinutes{kDefaultRetentionTimeMinutes};

    int segmentCount{kDefaultSegmentCount};
};

/**
 * @brief Convert language enum to string identifier.
 *
 * @param lang Language value.
 * @return Corresponding string ("system", "en", "fr", "es", "de", "zh_CN", "pt_BR").
 */
inline constexpr const char* to_string(Language lang)
{
    switch (lang)
    {
        case Language::System:
            return "system";
        case Language::English:
            return "en";
        case Language::French:
            return "fr";
        case Language::Spanish:
            return "es";
        case Language::German:
            return "de";
        case Language::ChineseSimplified:
            return "zh_CN";
        case Language::PortugueseBrazil:
            return "pt_BR";
    }
    return "system";
}

/**
 * @brief Convert string identifier to Language enum.
 *
 * @param s Input string.
 * @return Corresponding Language value (defaults to System).
 */
inline Language languageFromString(std::string_view s)
{
    if (s == "en")
        return Language::English;
    if (s == "fr")
        return Language::French;
    if (s == "es")
        return Language::Spanish;
    if (s == "de")
        return Language::German;
    if (s == "zh_CN")
        return Language::ChineseSimplified;
    if (s == "pt_BR")
        return Language::PortugueseBrazil;
    return Language::System;
}

/**
 * @brief Session type.
 *
 * Defines the current processing context.
 */
enum class Session
{
    Image, ///< Static image processing
    Video  ///< Dynamic video processing
};

/**
 * @brief Image drawn under the contours.
 *
 * Selects which image representation is shown in the UI and used for
 * exports (snapshots, recordings).
 */
enum class BackgroundImage
{
    Source,      ///< Original input image
    Preprocessed ///< Processed image after pipeline
};

/**
 * @brief Convert background image to string.
 */
inline constexpr const char* to_string(BackgroundImage mode)
{
    switch (mode)
    {
        case BackgroundImage::Source:
            return "source";
        case BackgroundImage::Preprocessed:
            return "preprocessed";
    }
    return "source";
}

/**
 * @brief Convert string to background image.
 */
inline BackgroundImage backgroundImageFromString(std::string_view s)
{
    if (s == "source")
        return BackgroundImage::Source;
    if (s == "preprocessed")
        return BackgroundImage::Preprocessed;
    return BackgroundImage::Source;
}

/**
 * @brief Style of the contour layers (content, also visible in exports).
 */
struct ContourStyleConfig
{
    static constexpr bool kDefaultVisible{true};
    static constexpr fluvel_ip::Rgb_uc kDefaultOuterColor{64u, 0u, 255u};
    static constexpr fluvel_ip::Rgb_uc kDefaultInnerColor{0u, 230u, 118u};

    bool outerVisible = kDefaultVisible;
    fluvel_ip::Rgb_uc outerColor{kDefaultOuterColor};

    bool innerVisible = kDefaultVisible;
    fluvel_ip::Rgb_uc innerColor{kDefaultInnerColor};
};

/**
 * @brief Presentation options of the image viewer (screen only, never exported).
 */
struct ViewConfig
{
    static constexpr bool kDefaultOptionDisabled{false};
    static constexpr bool kDefaultOverlayEnabled{true};
    static constexpr int kDefaultMiniMapSize{160};

    bool mirrorMode{kDefaultOptionDisabled};
    bool smoothDisplay{kDefaultOptionDisabled};

    bool algorithmOverlayEnabled{kDefaultOverlayEnabled};
    bool miniMapEnabled{kDefaultOverlayEnabled};

    /// Side of the (square) mini-map in pixels. Not saved in the user settings:
    /// a window that needs another size sets it when it builds its viewer.
    int miniMapSize{kDefaultMiniMapSize};
};

/**
 * @brief Display configuration for rendering images and contours.
 *
 * Content options (background, contour) decide what is shown and also
 * apply to exports. View options only change how the viewer presents it.
 */
struct DisplayConfig
{
    static constexpr BackgroundImage kDefaultBackground{BackgroundImage::Preprocessed};

    BackgroundImage background = kDefaultBackground;

    ContourStyleConfig contour{};
    ViewConfig view{};
};

/**
 * @brief Parameters controlling optional image downscaling.
 *
 * Used to reduce computation cost by processing a lower-resolution image.
 */
struct DownscaleParams
{
    static constexpr bool kDefaultDownscaleEnabled{false};
    static constexpr int kDefaultDownscaleFactor{2};

    bool downscaleEnabled = kDefaultDownscaleEnabled;
    int downscaleFactor = kDefaultDownscaleFactor;
};

/**
 * @brief Parameters controlling optional image downscaling.
 *
 * Used to reduce computation cost by processing a lower-resolution image.
 */
struct ActiveContourConfig
{
    static constexpr fluvel_ip::Connectivity kDefaultConnectivity{fluvel_ip::Connectivity::Four};

    fluvel_ip::Connectivity connectivity = kDefaultConnectivity;
    fluvel_ip::ActiveContourParams contourParams{};
    fluvel_ip::RegionColorParams regionParams{};
};

/**
 * @brief Configuration for image processing pipeline.
 *
 * Combines active contour parameters, preprocessing, and optional downscaling.
 */
struct ImageComputeConfig
{
    ActiveContourConfig contourConfig{};

    QImage initialPhi;

    DownscaleParams downscale{};
    fluvel_ip::ProcessingParams processing{};
};

/**
 * @brief Complete settings for an image session.
 *
 * Includes both computation and display configuration.
 */
struct ImageSessionSettings
{
    ImageComputeConfig compute{};
    DisplayConfig display{};
};

/**
 * @brief Complete settings for an image session.
 *
 * Includes both computation and display configuration.
 */
struct VideoComputeConfig
{
    static constexpr bool kDefaultSpatialFilteringEnabled{true};
    static constexpr bool kDefaultTemporalFilteringEnabled{true};

    DownscaleParams downscale{};
    bool spatialFilteringEnabled = kDefaultSpatialFilteringEnabled;
    bool temporalFilteringEnabled = kDefaultTemporalFilteringEnabled;

    ActiveContourConfig contourConfig{};

    VideoComputeConfig()
    {
        // Video mode: prefer robustness over strict failure stopping
        contourConfig.contourParams.failureMode = fluvel_ip::FailureHandlingMode::RecoverOnFailure;
    }
};

/**
 * @brief Complete settings for a video session.
 *
 * Includes both computation and display configuration.
 */
struct VideoSessionSettings
{
    VideoComputeConfig compute{};
    DisplayConfig display{};
};

} // namespace fluvel
