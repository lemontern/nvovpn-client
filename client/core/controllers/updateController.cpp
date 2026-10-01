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

#include <QCryptographicHash>
#include <QUrl>

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
        "https://ru.netguarder.net/api/v1/app/version",   // 01.10.2026: прямой российский вход — третья база, как в NvoApiController
        "https://nvovpn.com/api/v1/app/version",
    };
    constexpr int kAppcastUrlCount = 3;
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
    m_manualCheck = false;
    fetchAppcast(0);
}

void UpdateController::checkForUpdatesManual()
{
    if (m_updateCheckRunning) {
        return;
    }
    m_updateCheckRunning = true;
    m_manualCheck = true;
    fetchAppcast(0);
}

void UpdateController::finishUpdateCheck(bool found, const QString &error)
{
    m_updateCheckRunning = false;
    emit updateCheckFinished(found, m_manualCheck, error);
    m_manualCheck = false;
}

// Хост, с которого appcast реально скачался, заведомо доступен из этой сети; у всех наших хостов один корень,
// поэтому /downloads/<файл> есть на каждом. nvovpn.com (как в appcast) в РФ режется по SNI.
QUrl UpdateController::downloadUrlVia(int appcastIdx) const
{
    QUrl url(m_downloadUrl);
    if (!url.isValid() || url.host().isEmpty()) {
        return url;
    }
    if (appcastIdx >= 0 && appcastIdx < kAppcastUrlCount) {
        const QUrl base = QUrl(QString::fromLatin1(kAppcastUrls[appcastIdx]));   // не QUrl base(...): компиляторы читали это как объявление функции
        url.setScheme(QStringLiteral("https"));
        url.setHost(base.host());
    }
    return url;
}

QString UpdateController::platformKey()
{
#if defined(Q_OS_WINDOWS)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_ANDROID)
    return QStringLiteral("android");
#elif defined(Q_OS_IOS)
    return QStringLiteral("ios");        // версия из App Store; store_url — ссылка на страницу приложения
#else
    return QString();   // Linux: канала нет
#endif
}

void UpdateController::fetchAppcast(int urlIdx)
{
    if (platformKey().isEmpty()) {
        finishUpdateCheck(false, tr("На этой платформе обновления ставятся вручную"));
        return;
    }
    if (urlIdx >= kAppcastUrlCount) {
        finishUpdateCheck(false, tr("Не удалось проверить обновления — сервер NvoVPN недоступен"));
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
        m_sha256 = platform.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        m_appcastUrlIdx = urlIdx;
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        const QString storeUrl = platform.value(QStringLiteral("store_url")).toString();
        if (!storeUrl.isEmpty()) {
            m_downloadUrl = storeUrl;   // Google Play / App Store
        }
#endif
        m_releaseDate = root.value(QStringLiteral("generated_at")).toString().left(10);
        // UpdateUiController::getChangelogText показывает строки, начиная с «### General».
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        m_changelogText = QStringLiteral("### General\n")
            + tr("Доступна версия %1. Нажмите «Обновить», чтобы открыть магазин приложений.").arg(m_version);
#else
        m_changelogText = QStringLiteral("### General\n")
            + tr("Доступна версия %1. Нажмите «Обновить», чтобы скачать установщик.").arg(m_version);
#endif
        const QString changelogUrl = root.value(QStringLiteral("changelog_url")).toString();
        if (!changelogUrl.isEmpty()) {
            m_changelogText += QStringLiteral("\n") + tr("Что нового: %1").arg(changelogUrl);
        }

        if (m_version.isEmpty() || !isNewVersionAvailable()) {
            logger.info() << "appcast: обновлений нет (текущая" << APP_VERSION << ", в канале" << m_version << ")";
            finishUpdateCheck(false);
            return;
        }
        logger.info() << "appcast: доступна версия" << m_version << m_downloadUrl;
        emit updateFound();
        finishUpdateCheck(true);
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
    // Открываем ссылку — человек ставит сам. macOS: ссылка на хосте, с которого пришёл appcast (nvovpn.com в РФ закрыт).
#if defined(Q_OS_MACOS)
    QDesktopServices::openUrl(downloadUrlVia(m_appcastUrlIdx));
#else
    QDesktopServices::openUrl(QUrl(m_downloadUrl));
#endif
    return;
#endif
#if defined(Q_OS_WINDOWS)
    // 01.10.2026 (аудит D-3): качаем с доступного хоста, только https, сверяем sha256 из appcast, об ошибке говорим человеку.
    downloadInstaller(downloadUrlVia(m_appcastUrlIdx), false);
#endif
}

#if defined(Q_OS_WINDOWS)
void UpdateController::downloadInstaller(const QUrl &url, bool lastTry)
{
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        logger.error() << "Installer URL rejected (not https):" << url.toString();
        emit installerFailed(tr("Ссылка на установщик некорректна. Скачайте его с сайта nvovpn.com"));
        return;
    }
    QNetworkRequest request;
    request.setTransferTimeout(180000);   // 112 МБ по медленной сети — 30 с не хватало
    request.setUrl(url);
    request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArray("NvoVPN/") + APP_VERSION + " (" + QSysInfo::prettyProductName().toUtf8() + ")");

    QNetworkReply *reply = amnApp->networkManager()->get(request);
    setupNetworkErrorHandling(reply, QStringLiteral("installer"));

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url, lastTry]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            logger.error() << "Installer download failed:" << url.toString() << static_cast<int>(reply->error()) << reply->errorString() << "HTTP" << status;
            if (!lastTry) {
                // Второй шанс — тот же файл на следующем из наших хостов.
                const int next = (m_appcastUrlIdx + 1) % kAppcastUrlCount;
                downloadInstaller(downloadUrlVia(next), true);
                return;
            }
            emit installerFailed(tr("Не удалось скачать обновление. Проверьте интернет или скачайте установщик с сайта"));
            return;
        }
        const QByteArray data = reply->readAll();
        if (data.size() < 10 * 1024 * 1024) {   // установщик ~110 МБ; страница-заглушка или обрезанный файл — не запускаем
            logger.error() << "Installer suspiciously small:" << data.size();
            emit installerFailed(tr("Файл обновления повреждён, попробуйте позже"));
            return;
        }
        if (!m_sha256.isEmpty()) {
            const QString actual = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
            if (actual != m_sha256) {
                logger.error() << "Installer sha256 mismatch: expected" << m_sha256 << "got" << actual;
                emit installerFailed(tr("Файл обновления не прошёл проверку, попробуйте позже"));
                return;
            }
        }
        QFile file(kInstallerLocalPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            logger.error() << "Failed to write installer file:" << kInstallerLocalPath << "Error:" << file.errorString();
            file.close();
            emit installerFailed(tr("Не удалось сохранить установщик, скачайте его с сайта"));
            return;
        }
        file.close();
        if (runWindowsInstaller(kInstallerLocalPath) != 0) {
            emit installerFailed(tr("Не удалось запустить установщик, скачайте его с сайта"));
        }
    });
}
#endif

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
