#include "languageUiController.h"

LanguageUiController::LanguageUiController(SettingsController* settingsController,
                                           LanguageModel* languageModel,
                                           QObject *parent)
    : QObject(parent),
      m_settingsController(settingsController),
      m_languageModel(languageModel)
{
}

void LanguageUiController::onAppLanguageChanged(const QLocale &locale)
{
    emit updateTranslations(locale);
}

void LanguageUiController::changeLanguage(const LanguageSettings::AvailableLanguageEnum language)
{
    QLocale locale = languageEnumToLocale(language);
    m_settingsController->setAppLanguage(locale);
}

int LanguageUiController::getCurrentLanguageIndex() const
{
    auto locale = m_settingsController->getAppLanguage();
    switch (locale.language()) {
    case QLocale::Russian: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Russian); break;
    case QLocale::English: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::English); break;
    case QLocale::German: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::German); break;
    case QLocale::Polish: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Polish); break;
    case QLocale::Italian: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Italian); break;
    case QLocale::Spanish: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Spanish); break;
    case QLocale::French: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::French); break;
    default: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Russian); break;
    }
}

int LanguageUiController::getLineHeightAppend() const
{
    auto locale = m_settingsController->getAppLanguage();
    switch (locale.language()) {
    case QLocale::Burmese: return 10; break;
    default: return 0; break;
    }
}

QString LanguageUiController::getCurrentLanguageName() const
{
    int index = getCurrentLanguageIndex();
    return getLocalLanguageName(static_cast<LanguageSettings::AvailableLanguageEnum>(index));
}

LanguageSettings::AvailableLanguageEnum LanguageUiController::getSystemLanguageEnum() const
{
    QLocale locale = QLocale::system();
    switch (locale.language()) {
    case QLocale::Russian: return LanguageSettings::AvailableLanguageEnum::Russian;
    case QLocale::English: return LanguageSettings::AvailableLanguageEnum::English;
    case QLocale::German: return LanguageSettings::AvailableLanguageEnum::German;
    case QLocale::Polish: return LanguageSettings::AvailableLanguageEnum::Polish;
    case QLocale::Italian: return LanguageSettings::AvailableLanguageEnum::Italian;
    case QLocale::Spanish: return LanguageSettings::AvailableLanguageEnum::Spanish;
    case QLocale::French: return LanguageSettings::AvailableLanguageEnum::French;
    default: return LanguageSettings::AvailableLanguageEnum::English;
    }
}

QString LanguageUiController::getCurrentSiteUrl(const QString &path) const
{
    auto locale = m_settingsController->getAppLanguage();
    if (locale.language() == QLocale::Russian) {
        return "https://storage.googleapis.com/amnezia/amnezia.org" + (path.isEmpty() ? "" : (QString("?m-path=/%1").arg(path)));
    }
    return QString("https://amnezia.org") + (path.isEmpty() ? "" : (QString("/%1").arg(path)));
}

QString LanguageUiController::getCurrentDocsUrl(const QString &path) const
{
    auto locale = m_settingsController->getAppLanguage();
    if (locale.language() == QLocale::Russian) {
        return "https://storage.googleapis.com/amnezia/docs" + (path.isEmpty() ? "" : (QString("?m-path=/%1").arg(path)));
    }
    return QString("https://docs.amnezia.org") + (path.isEmpty() ? "" : (QString("/%1").arg(path)));
}

QString LanguageUiController::getLocalLanguageName(const LanguageSettings::AvailableLanguageEnum language) const
{
    QString strLanguage("");
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::Russian: strLanguage = "Русский"; break;
    case LanguageSettings::AvailableLanguageEnum::English: strLanguage = "English"; break;
    case LanguageSettings::AvailableLanguageEnum::German: strLanguage = "Deutsch"; break;
    case LanguageSettings::AvailableLanguageEnum::Polish: strLanguage = "Polski"; break;
    case LanguageSettings::AvailableLanguageEnum::Italian: strLanguage = "Italiano"; break;
    case LanguageSettings::AvailableLanguageEnum::Spanish: strLanguage = "Español"; break;
    case LanguageSettings::AvailableLanguageEnum::French: strLanguage = "Français"; break;
    default: break;
    }

    return strLanguage;
}

QLocale LanguageUiController::languageEnumToLocale(const LanguageSettings::AvailableLanguageEnum language) const
{
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::Russian: return QLocale::Russian;
    case LanguageSettings::AvailableLanguageEnum::English: return QLocale::English;
    case LanguageSettings::AvailableLanguageEnum::German: return QLocale::German;
    case LanguageSettings::AvailableLanguageEnum::Polish: return QLocale::Polish;
    case LanguageSettings::AvailableLanguageEnum::Italian: return QLocale::Italian;
    case LanguageSettings::AvailableLanguageEnum::Spanish: return QLocale::Spanish;
    case LanguageSettings::AvailableLanguageEnum::French: return QLocale::French;
    default: return QLocale::Russian;
    }
}

