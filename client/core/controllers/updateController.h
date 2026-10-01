#ifndef UPDATECONTROLLER_H
#define UPDATECONTROLLER_H

#include <functional>
#include <QObject>
#include <QNetworkReply>
#include <QUrl>

#include "core/repositories/secureAppSettingsRepository.h"

class UpdateController : public QObject
{
    Q_OBJECT
public:
    explicit UpdateController(SecureAppSettingsRepository* appSettingsRepository, QObject *parent = nullptr);

    QString getRawChangelogText() const;
    QString getReleaseDate() const;
    QString getVersion() const;

public slots:
    void checkForUpdates();
    void checkForUpdatesManual();               // 01.10.2026: по кнопке «Проверить обновления» — с ответом «у вас последняя версия»
    void runInstaller();

signals:
    void updateFound();
    void updateCheckFinished(bool found, bool manual, const QString &error);   // 01.10.2026: итог проверки (для тоста по кнопке)
    void installerFailed(const QString &message);                              // 01.10.2026: скачивание/проверка установщика не удались

private:
    void finishUpdateCheck(bool found = false, const QString &error = QString());
    void fetchAppcast(int urlIdx);              // NvoVPN: GET /api/v1/app/version (appcast.json), резервный домен при ошибке
    QUrl downloadUrlVia(int appcastIdx) const;  // 01.10.2026: ссылка на установщик на хосте, с которого пришёл appcast (nvovpn.com в РФ закрыт)
#if defined(Q_OS_WINDOWS)
    void downloadInstaller(const QUrl &url, bool lastTry);   // 01.10.2026: скачать, сверить sha256 из appcast, запустить
#endif
    static QString platformKey();               // ключ платформы в appcast: windows | macos | android
    bool isNewVersionAvailable() const;
    void setupNetworkErrorHandling(QNetworkReply* reply, const QString& operation);
    void handleNetworkError(QNetworkReply* reply, const QString& operation);

    SecureAppSettingsRepository* m_appSettingsRepository;

    QString m_changelogText;
    QString m_version;
    QString m_releaseDate;
    QString m_downloadUrl;
    QString m_sha256;                           // 01.10.2026: sha256 установщика из appcast (пусто — не проверяем)
    int m_appcastUrlIdx = 0;                    // с какого из kAppcastUrls пришёл appcast — тот хост точно доступен
    bool m_updateCheckRunning = false;
    bool m_manualCheck = false;

#if defined(Q_OS_WINDOWS)
    int runWindowsInstaller(const QString &installerPath);
#elif defined(Q_OS_MACOS)
    int runMacInstaller(const QString &installerPath);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int runLinuxInstaller(const QString &installerPath);
#endif
};

#endif // UPDATECONTROLLER_H
