#ifndef UPDATECONTROLLER_H
#define UPDATECONTROLLER_H

#include <functional>
#include <QObject>
#include <QNetworkReply>

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
    void runInstaller();

signals:
    void updateFound();

private:
    void finishUpdateCheck();
    void fetchAppcast(int urlIdx);              // NvoVPN: GET /api/v1/app/version (appcast.json), резервный домен при ошибке
    static QString platformKey();               // ключ платформы в appcast: windows | macos | android
    bool isNewVersionAvailable() const;
    void setupNetworkErrorHandling(QNetworkReply* reply, const QString& operation);
    void handleNetworkError(QNetworkReply* reply, const QString& operation);

    SecureAppSettingsRepository* m_appSettingsRepository;

    QString m_changelogText;
    QString m_version;
    QString m_releaseDate;
    QString m_downloadUrl;
    bool m_updateCheckRunning = false;

#if defined(Q_OS_WINDOWS)
    int runWindowsInstaller(const QString &installerPath);
#elif defined(Q_OS_MACOS)
    int runMacInstaller(const QString &installerPath);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    int runLinuxInstaller(const QString &installerPath);
#endif
};

#endif // UPDATECONTROLLER_H
