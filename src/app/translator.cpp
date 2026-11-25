// SPDX-License-Identifier: CeCILL-2.1
// Copyright (C) 2010-2026 Fabien Bessy

#include "translator.hpp"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>

namespace fluvel
{

namespace
{

QLocale localeFor(Language language)
{
    switch (language)
    {
        case Language::French:
            return QLocale(QLocale::French);
        case Language::English:
            return QLocale(QLocale::English);
        case Language::Spanish:
            return QLocale(QLocale::Spanish);
        case Language::German:
            return QLocale(QLocale::German);
        case Language::ChineseSimplified:
            return QLocale(QLocale::Chinese, QLocale::SimplifiedChineseScript, QLocale::China);
        case Language::PortugueseBrazil:
            return QLocale(QLocale::Portuguese, QLocale::Brazil);
        case Language::PortuguesePortugal:
            return QLocale(QLocale::Portuguese, QLocale::Portugal);
        case Language::Italian:
            return QLocale(QLocale::Italian);
        case Language::Russian:
            return QLocale(QLocale::Russian);
        case Language::Japanese:
            return QLocale(QLocale::Japanese);
        case Language::Korean:
            return QLocale(QLocale::Korean);
        case Language::System:
        default:
            return QLocale::system();
    }
}

} // namespace

Translator::Translator(QCoreApplication& app, Language language)
{
    const QLocale locale = localeFor(language);

    // QTranslator::load(QLocale, ...) tries the locale UI languages from the most
    // to the least specific (e.g. fluvel_pt_BR.qm, then fluvel_pt.qm).
    if (qtTranslator_.load(locale, QStringLiteral("qt"), QStringLiteral("_"),
                           QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        app.installTranslator(&qtTranslator_);
    }

    // No file is expected for English, the source language.
    if (appTranslator_.load(locale, QStringLiteral("fluvel"), QStringLiteral("_"),
                            QStringLiteral(":/i18n")))
    {
        app.installTranslator(&appTranslator_);
    }
}

} // namespace fluvel
