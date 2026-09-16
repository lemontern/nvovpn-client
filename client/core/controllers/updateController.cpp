#include "updateController.h"

#include <QDesktopServices>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QStandardPaths>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include "amneziaApplication.h"
#include "logger.h"
#include "version.h"
#include "core/utils/selfhosted/scriptsRegistry.h"

namespace
{
    Logger logger("UpdateController");

#if defined(Q_OS_WINDOWS)
    const QString kInstallerLocalPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/NvoVPN_installer.exe";
#endif

    // NvoVPN (16.09.2026): канал обновлений — наш appcast (routes/api.php /v1/app/version, файл
    // public/downloads/appcast.json, генерирует /root/nvovpn_appcast.sh). Шлюз Amnezia (updater_endpoint)
    // у нас не работал никогда — секрета PROD_AGW_PUBLIC_KEY нет, поэтому десктоп не узнавал об обновлениях.
    // Основная база — незаблокированный в РФ домен, nvovpn.com — резерв.
    constexpr const char *kAppcastUrls[] = {
        "https://api.netguarder.net/api/v1/app/version",
        "https://nvovpn.com/api/v1/app/version",
    };
    constexpr int kAppcastUrlCount = 2;
}

UpdateController::UpdateController(SecureAppSettingsRepository* appSettingsRepository, QObject *parent)
    : QObject(parent), m_appSettingsRepository(appSettingsRepository)
{
}

QString UpdateController::getRawChangelogText() const
{
    return m_changelogText;
}

QString UpdateController::getReleaseDate() const
{
    return m_releaseDate;
}

QString UpdateController::getVersion() const
{
    return m_version;
}

void UpdateController::checkForUpdates()
{
    if (m_updateCheckRunning) {
        return;
    }
    m_updateCheckRunning = true;
    fetchAppcast(0);
}

void UpdateController::finishUpdateCheck()
{
    m_updateCheckRunning = false;
}

QString UpdateController::platformKey()
{
#if defined(Q_OS_WINDOWS)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#else
    return QString();   // Linux/iOS: канала нет (iOS обновляет App Store)
#endif
}

void UpdateController::fetchAppcast(int urlIdx)
{
    if (urlIdx >= kAppcastUrlCount || platformKey().isEmpty()) {
        finishUpdateCheck();
        return;
    }

    QNetworkRequest req;
    req.setTransferTimeout(7000);
    req.setUrl(QUrl(QLatin1String(kAppcastUrls[urlIdx])));
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    req.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArray("NvoVPN/") + APP_VERSION + " (" + QSysInfo::prettyProductName().toUtf8() + ")");

    QNetworkReply *reply = amnApp->networkManager()->get(req);
    setupNetworkErrorHandling(reply, QStringLiteral("appcast"));

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, urlIdx]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        const QByteArray data = ok ? reply->readAll() : QByteArray();
        if (!ok) {
            handleNetworkError(reply, QStringLiteral("appcast"));
        }
        reply->deleteLater();
        if (!ok) {
            fetchAppcast(urlIdx + 1);   // резервный домен
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject platform = root.value(QStringLiteral("platforms")).toObject().value(platformKey()).toObject();
        m_version = platform.value(QStringLiteral("version")).toString().trimmed();
        m_downloadUrl = platform.value(QStringLiteral("url")).toString();
#if defined(Q_OS_ANDROID)
        const QString storeUrl = platform.value(QStringLiteral("store_url")).toString();
        if (!storeUrl.isEmpty()) {
            m_downloadUrl = storeUrl;
        }
#endif
        m_releaseDate = root.value(QStringLiteral("generated_at")).toString().left(10);
        // UpdateUiController::getChangelogText показывает строки, начиная с «### General».
        m_changelogText = QStringLiteral("### General\n")
            + tr("Доступна версия %1. Нажмите «Обновить», чтобы скачать установщик.").arg(m_version);
        const QString changelogUrl = root.value(QStringLiteral("changelog_url")).toString();
        if (!changelogUrl.isEmpty()) {
            m_changelogText += QStringLiteral("\n") + tr("Что нового: %1").arg(changelogUrl);
        }

        if (m_version.isEmpty() || !isNewVersionAvailable()) {
            logger.info() << "appcast: обновлений нет (текущая" << APP_VERSION << ", в канале" << m_version << ")";
            finishUpdateCheck();
            return;
        }
        logger.info() << "appcast: доступна версия" << m_version << m_downloadUrl;
        emit updateFound();
        finishUpdateCheck();
    });
}

bool UpdateController::isNewVersionAvailable() const
{
    auto currentVersion = QVersionNumber::fromString(QString(APP_VERSION));
    auto newVersion = QVersionNumber::fromString(m_version);
    return newVersion > currentVersion;
}

void UpdateController::setupNetworkErrorHandling(QNetworkReply* reply, const QString& operation)
{
    QObject::connect(reply, &QNetworkReply::errorOccurred, [reply, operation](QNetworkReply::NetworkError error) {
        logger.error() << QString("Network error occurred while fetching %1: %2 %3")
                          .arg(operation, reply->errorString(), QString::number(error));
    });

    QObject::connect(reply, &QNetworkReply::sslErrors, [operation](const QList<QSslError> &errors) {
        QStringList errorStrings;
        for (const QSslError &err : errors) {
            errorStrings << err.errorString();
        }
        logger.error() << QString("SSL errors while fetching %1: %2").arg(operation, errorStrings.join("; "));
    });
}

void UpdateController::handleNetworkError(QNetworkReply* reply, const QString& operation)
{
    logger.error() << "Network error code:" << QString::number(static_cast<int>(reply->error()));
    logger.error() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

void UpdateController::runInstaller()
{
    if (m_downloadUrl.isEmpty()) {
        logger.error() << "Download URL is empty";
        return;
    }
#if defined(Q_OS_MACOS) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    // NvoVPN: macOS раздаётся как .dmg с сайта (скрипт mac_installer.sh рассчитан на .pkg), Android — Google Play.
    // Открываем ссылку — человек ставит сам.
    QDesktopServices::openUrl(QUrl(m_downloadUrl));
    return;
#endif
#if defined(Q_OS_WINDOWS)

    QNetworkRequest request;
    request.setTransferTimeout(30000);
    request.setUrl(m_downloadUrl);

    QNetworkReply *reply = amnApp->networkManager()->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(kInstallerLocalPath);
            if (!file.open(QIODevice::WriteOnly)) {
                logger.error() << "Failed to open installer file for writing:" << kInstallerLocalPath << "Error:" << file.errorString();
                reply->deleteLater();
                return;
            }

            if (file.write(reply->readAll()) == -1) {
                logger.error() << "Failed to write installer data to file:" << kInstallerLocalPath << "Error:" << file.errorString();
                file.close();
                reply->deleteLater();
                return;
            }

            file.close();

            runWindowsInstaller(kInstallerLocalPath);
        } else {
            logger.error() << "Installer download failed, network error:" << static_cast<int>(reply->error())
                           << reply->errorString();
            logger.error() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        }
        reply->deleteLater();
    });
#endif
}

#if defined(Q_OS_WINDOWS)
int UpdateController::runWindowsInstaller(const QString &installerPath)
{
    qint64 pid;
    bool success = QProcess::startDetached(installerPath, QStringList(), QString(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_MACOS) && !defined(MACOS_NE)
int UpdateController::runMacInstaller(const QString &installerPath)
{
    // Create temporary directory for extraction
    QTemporaryDir extractDir;
    extractDir.setAutoRemove(false);
    if (!extractDir.isValid()) {
        logger.error() << "Failed to create temporary directory";
        return -1;
    }
    logger.info() << "Temporary directory created:" << extractDir.path();

    // Create script file in the temporary directory
    QString scriptPath = extractDir.path() + "/mac_installer.sh";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly)) {
        logger.error() << "Failed to create script file";
        return -1;
    }

    // Get script content from registry
    QString scriptContent = amnezia::scriptData(amnezia::ClientScriptType::mac_installer);
    if (scriptContent.isEmpty()) {
        logger.error() << "macOS installer script content is empty";
        scriptFile.close();
        return -1;
    }

    scriptFile.write(scriptContent.toUtf8());
    scriptFile.close();
    logger.info() << "Script file created:" << scriptPath;

    // Make script executable
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeUser);

    // Start detached process
    qint64 pid;
    bool success =
            QProcess::startDetached("/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
int UpdateController::runLinuxInstaller(const QString &installerPath)
{
    QFile::setPermissions(installerPath, QFile::permissions(installerPath) | QFile::ExeUser);

    qint64 pid;
    bool success = QProcess::startDetached(installerPath, QStringList(), QString(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif
