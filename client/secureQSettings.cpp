#include "secureQSettings.h"

#include "../client/3rd/QSimpleCrypto/src/include/QAead.h"
#include "../client/3rd/QSimpleCrypto/src/include/QBlockCipher.h"
#include "core/utils/utilities.h"
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>

using namespace QKeychain;

namespace {
    constexpr const char *settingsKeyTag = "settingsKeyTag";
    constexpr const char *settingsIvTag = "settingsIvTag";
    constexpr const char *keyChainName = "AmneziaVPN-Keychain";
}

SecureQSettings::SecureQSettings(const QString &organization, const QString &application, QObject *parent, bool enableEncryption)
    : QObject { parent }, m_settings(organization, application, parent), encryptedKeys({ "Servers/serversList", "Conf/nvoToken" }), m_encryptionEnabled(enableEncryption)
{
    bool encrypted = m_settings.value("Conf/encrypted").toBool();

    // convert settings to encrypted for if updated to >= 2.1.0
    if (encryptionRequired() && !encrypted) {
        for (const QString &key : m_settings.allKeys()) {
            if (encryptedKeys.contains(key)) {
                const QVariant &val = value(key);
                setValue(key, val);
            }
        }
        m_settings.setValue("Conf/encrypted", true);
    }
}

QVariant SecureQSettings::value(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&m_mutex);

    if (m_cache.contains(key)) {
        return m_cache.value(key);
    }

    if (!m_settings.contains(key))
        return defaultValue;

    QVariant retVal;

    // check if value is not encrypted, v. < 2.0.x
    retVal = m_settings.value(key);
    if (retVal.isValid()) {
        if (retVal.userType() == QMetaType::QByteArray && retVal.toByteArray().mid(0, magicString.size()) == magicString) {

            if (getEncKey().isEmpty() || getEncIv().isEmpty()) {
                qCritical() << "SecureQSettings::setValue Decryption requested, but key is empty";
                return {};
            }

            QByteArray encryptedValue = retVal.toByteArray().mid(magicString.size());

            QByteArray decryptedValue = decryptText(encryptedValue);
            QDataStream ds(&decryptedValue, QIODevice::ReadOnly);

            ds >> retVal;

            if (!retVal.isValid()) {
                qWarning() << "SecureQSettings::value settings decryption failed";
                retVal = QVariant();
            }
        }
    } else {
        qWarning() << "SecureQSettings::value invalid QVariant value";
        retVal = QVariant();
    }

    m_cache.insert(key, retVal);
    return retVal;
}

void SecureQSettings::setValue(const QString &key, const QVariant &value)
{
    QMutexLocker locker(&m_mutex);

    if (encryptionRequired() && encryptedKeys.contains(key)) {
        if (!getEncKey().isEmpty() && !getEncIv().isEmpty()) {
            QByteArray decryptedValue;
            {
                QDataStream ds(&decryptedValue, QIODevice::WriteOnly);
                ds << value;
            }

            QByteArray encryptedValue = encryptText(decryptedValue);
            m_settings.setValue(key, magicString + encryptedValue);
        } else {
            qCritical() << "SecureQSettings::setValue Encryption required, but key is empty";
            return;
        }

    } else {
        m_settings.setValue(key, value);
    }

    m_cache.insert(key, value);
}

void SecureQSettings::remove(const QString &key)
{
    QMutexLocker locker(&m_mutex);

    m_settings.remove(key);
    m_cache.remove(key);
}

QByteArray SecureQSettings::backupAppConfig() const
{
    QMutexLocker locker(&m_mutex);

    QJsonObject cfg;

    const auto needToBackup = [this](const auto &key) {
      for (const auto &item : m_fieldsToBackup)
      {
        if (key == "Conf/installationUuid")
        {
          return false;
        }

        if (key.startsWith(item))
        {
            return true;
        }
      }

      return false;
    };

    for (const QString &key : m_settings.allKeys()) {

        if (!needToBackup(key))
        {
            continue;
        }

        cfg.insert(key, QJsonValue::fromVariant(value(key)));
    }

    return QJsonDocument(cfg).toJson();
}

bool SecureQSettings::restoreAppConfig(const QByteArray &json)
{
    QMutexLocker locker(&m_mutex);

    QJsonObject cfg = QJsonDocument::fromJson(json).object();
    if (cfg.isEmpty())
        return false;

    for (const QString &key : cfg.keys()) {
        if (key == "Conf/installationUuid") {
            continue;
        }

        setValue(key, cfg.value(key).toVariant());
    }

    return true;
}

void SecureQSettings::clearSettings()
{
    QMutexLocker locker(&m_mutex);
    m_settings.clear();
    m_cache.clear();
}

QByteArray SecureQSettings::encryptText(const QByteArray &value) const
{
    QSimpleCrypto::QBlockCipher cipher;
    QByteArray result;
    try {
        result = cipher.encryptAesBlockCipher(value, getEncKey(), getEncIv());
    } catch (...) { // todo change error handling in QSimpleCrypto?
        qCritical() << "error when encrypting the settings value";
    }
    return result;
}

QByteArray SecureQSettings::decryptText(const QByteArray &ba) const
{
    QSimpleCrypto::QBlockCipher cipher;
    QByteArray result;
    try {
        result = cipher.decryptAesBlockCipher(ba, getEncKey(), getEncIv());
    } catch (...) { // todo change error handling in QSimpleCrypto?
        qCritical() << "error when decrypting the settings value";
    }
    return result;
}

bool SecureQSettings::encryptionRequired() const
{
    if (!m_encryptionEnabled) {
        return false;
    }
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // On Linux, QtKeychain may fail if no secret service (e.g. gnome-keyring) is running.
    // Encryption is still attempted — getEncKey()/getEncIv() will log errors if keychain
    // is unavailable, and setValue() will skip storing rather than save in plaintext.
    // A file-based key fallback should be implemented for headless/minimal Linux environments.
#endif
    return true;
}

QByteArray SecureQSettings::getEncKey() const
{

    if (!m_key.isEmpty()) {
        return m_key;
    }
    // load keys from system key storage
    m_key = getSecTag(settingsKeyTag);

    if (m_key.isEmpty()) {
        // Create new key
        QSimpleCrypto::QBlockCipher cipher;
        QByteArray key = cipher.generatePrivateSalt(32);
        if (key.isEmpty()) {
            qCritical() << "SecureQSettings::getEncKey Unable to generate new enc key";
        }

        setSecTag(settingsKeyTag, key);

        // check
        m_key = getSecTag(settingsKeyTag);
        if (key != m_key) {
            qCritical() << "SecureQSettings::getEncKey Unable to store key in keychain" << key.size() << m_key.size();
            return {};
        }
    }

    return m_key;
}

QByteArray SecureQSettings::getEncIv() const
{
    if (!m_iv.isEmpty()) {
        return m_iv;
    }
    // load keys from system key storage
    m_iv = getSecTag(settingsIvTag);

    if (m_iv.isEmpty()) {
        // Create new IV
        QSimpleCrypto::QBlockCipher cipher;
        QByteArray iv = cipher.generatePrivateSalt(32);
        if (iv.isEmpty()) {
            qCritical() << "SecureQSettings::getEncIv Unable to generate new enc IV";
        }
        setSecTag(settingsIvTag, iv);

        // check
        m_iv = getSecTag(settingsIvTag);
        if (iv != m_iv) {
            qCritical() << "SecureQSettings::getEncIv Unable to store IV in keychain" << iv.size() << m_iv.size();
            return {};
        }
    }

    return m_iv;
}

QByteArray SecureQSettings::getSecTag(const QString &tag)
{
    auto job = QSharedPointer<ReadPasswordJob>(new ReadPasswordJob(keyChainName), &QObject::deleteLater);
    job->setAutoDelete(false);
    job->setKey(tag);
    QEventLoop loop;
    job->connect(job.data(), &ReadPasswordJob::finished, job.data(), [&loop]() { loop.quit(); });
    job->start();
    loop.exec();

    if (job->error()) {
        qCritical() << "SecureQSettings::getSecTag Error:" << job->errorString();
        // Fallback: try file-based storage when keychain is unavailable
        return getSecTagFromFile(tag);
    }

    return job->binaryData();
}

void SecureQSettings::setSecTag(const QString &tag, const QByteArray &data)
{
    auto job = QSharedPointer<WritePasswordJob>(new WritePasswordJob(keyChainName), &QObject::deleteLater);
    job->setAutoDelete(false);
    job->setKey(tag);
    job->setBinaryData(data);
    QEventLoop loop;
    QTimer::singleShot(1000, &loop, SLOT(quit()));
    job->connect(job.data(), &WritePasswordJob::finished, job.data(), [&loop]() { loop.quit(); });
    job->start();
    loop.exec();

    if (job->error()) {
        qCritical() << "SecureQSettings::setSecTag Error:" << job->errorString();
        // Fallback: use file-based storage when keychain is unavailable
        setSecTagToFile(tag, data);
    }
}

QString SecureQSettings::secTagFilePath(const QString &tag)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/.keys";
    QDir().mkpath(dir);
    return dir + "/" + QString(tag).replace('/', '_');
}

QByteArray SecureQSettings::getSecTagFromFile(const QString &tag)
{
    QFile file(secTagFilePath(tag));
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "SecureQSettings::getSecTagFromFile Cannot read key file";
        return {};
    }
    return file.readAll();
}

void SecureQSettings::setSecTagToFile(const QString &tag, const QByteArray &data)
{
    const QString path = secTagFilePath(tag);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "SecureQSettings::setSecTagToFile Cannot write key file";
        return;
    }
    file.write(data);
    file.close();
    // Restrict permissions to owner-only (0600)
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
}
