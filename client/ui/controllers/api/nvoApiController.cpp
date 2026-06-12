#include "nvoApiController.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>

#include "secureQSettings.h"
#include "ui/models/api/nvoServersModel.h"
#include "logger.h"

namespace
{
    Logger logger("NvoApiController");

    constexpr char API_BASE[] = "https://nvovpn.com/api/v1";
    constexpr char TOKEN_KEY[] = "Conf/nvoToken";

    int httpStatus(QNetworkReply *reply)
    {
        return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
}

NvoApiController::NvoApiController(SecureQSettings *settings, NvoServersModel *serversModel, QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_settings(settings),
      m_serversModel(serversModel)
{
    if (m_settings) {
        m_token = QString::fromUtf8(m_settings->value(QString::fromLatin1(TOKEN_KEY)).toByteArray());
    }
}

bool NvoApiController::isAuthenticated() const { return !m_token.isEmpty(); }
bool NvoApiController::isBusy() const { return m_busy; }
QString NvoApiController::userName() const { return m_userName; }
QString NvoApiController::userEmail() const { return m_userEmail; }
bool NvoApiController::hasSubscription() const { return m_hasSubscription; }
QString NvoApiController::subscriptionPlan() const { return m_subPlan; }
QString NvoApiController::subscriptionStatus() const { return m_subStatus; }
QString NvoApiController::subscriptionExpiresAt() const { return m_subExpiresAt; }
int NvoApiController::subscriptionDaysRemaining() const { return m_subDaysRemaining; }
QString NvoApiController::token() const { return m_token; }

QNetworkRequest NvoApiController::makeRequest(const QString &path, bool auth) const
{
    QNetworkRequest req(QUrl(QString::fromLatin1(API_BASE) + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
    if (auth && !m_token.isEmpty()) {
        req.setRawHeader(QByteArrayLiteral("Authorization"), QByteArray("Bearer ") + m_token.toUtf8());
    }
    return req;
}

void NvoApiController::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void NvoApiController::setToken(const QString &token)
{
    const bool wasAuth = !m_token.isEmpty();
    m_token = token;
    if (m_settings) {
        if (token.isEmpty()) {
            m_settings->remove(QString::fromLatin1(TOKEN_KEY));
        } else {
            m_settings->setValue(QString::fromLatin1(TOKEN_KEY), token.toUtf8());
        }
    }
    if (wasAuth != !m_token.isEmpty()) {
        emit authenticationChanged();
    }
}

void NvoApiController::login(const QString &email, const QString &password)
{
    setBusy(true);
    const QJsonObject body { { "email", email }, { "password", password } };
    QNetworkReply *reply = m_nam->post(makeRequest(QStringLiteral("/auth/login"), false),
                                       QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);
        const int status = httpStatus(reply);
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            emit loginFailed(status == 401 ? tr("Неверный email или пароль") : humanError(reply));
            return;
        }
        const QString token = root.value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            emit loginFailed(tr("Не удалось войти, попробуйте ещё раз"));
            return;
        }
        setToken(token);
        applyUser(root);
        emit loginSucceeded();
        refreshUser();
        refreshServers();
    });
}

void NvoApiController::loginByCode(const QString &code)
{
    setBusy(true);
    const QJsonObject body { { "code", code } };
    QNetworkReply *reply = m_nam->post(makeRequest(QStringLiteral("/auth/login/code"), false),
                                       QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);
        const int status = httpStatus(reply);
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            emit loginFailed(status == 401 || status == 404 ? tr("Код неверный или истёк") : humanError(reply));
            return;
        }
        const QString token = root.value(QStringLiteral("token")).toString();
        if (token.isEmpty()) {
            emit loginFailed(tr("Не удалось войти, попробуйте ещё раз"));
            return;
        }
        setToken(token);
        applyUser(root);
        emit loginSucceeded();
        refreshUser();
        refreshServers();
    });
}

void NvoApiController::logout()
{
    if (!m_token.isEmpty()) {
        QNetworkReply *reply = m_nam->post(makeRequest(QStringLiteral("/auth/logout"), true), QByteArray());
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    }
    setToken(QString());

    m_userName.clear();
    m_userEmail.clear();
    m_hasSubscription = false;
    m_subPlan.clear();
    m_subStatus.clear();
    m_subExpiresAt.clear();
    m_subDaysRemaining = 0;
    emit userChanged();
    emit subscriptionChanged();

    if (m_serversModel) {
        m_serversModel->updateModel(QJsonArray());
    }
}

void NvoApiController::refreshServers()
{
    if (m_token.isEmpty()) {
        return;
    }
    setBusy(true);
    QNetworkReply *reply = m_nam->get(makeRequest(QStringLiteral("/servers"), true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);
        const int status = httpStatus(reply);
        if (status == 401) {
            setToken(QString());
            emit errorOccurred(tr("Сессия истекла, войдите снова"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(humanError(reply));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (m_serversModel) {
            m_serversModel->updateModel(root.value(QStringLiteral("servers")).toArray());
        }
        emit serversUpdated();
    });
}

void NvoApiController::refreshUser()
{
    if (m_token.isEmpty()) {
        return;
    }
    QNetworkReply *reply = m_nam->get(makeRequest(QStringLiteral("/user"), true));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int status = httpStatus(reply);
        if (status == 401) {
            setToken(QString());
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        applyUser(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void NvoApiController::requestConfig(int serverId)
{
    if (m_token.isEmpty()) {
        emit errorOccurred(tr("Сначала войдите в аккаунт"));
        return;
    }
    setBusy(true);
    const QJsonObject body { { "server_id", serverId }, { "protocol", QStringLiteral("amneziawg") } };
    QNetworkReply *reply = m_nam->post(makeRequest(QStringLiteral("/connect"), true),
                                       QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, serverId]() {
        reply->deleteLater();
        setBusy(false);
        const int status = httpStatus(reply);
        if (status == 403) {
            emit subscriptionRequired();
            return;
        }
        if (status == 401) {
            setToken(QString());
            emit errorOccurred(tr("Сессия истекла, войдите снова"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(humanError(reply));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString config = root.value(QStringLiteral("config")).toString();
        if (config.isEmpty()) {
            emit errorOccurred(tr("Не удалось получить настройки сервера"));
            return;
        }
        const QJsonObject server = root.value(QStringLiteral("server")).toObject();
        emit configReady(config, serverId, server.value(QStringLiteral("name")).toString(),
                         root.value(QStringLiteral("vpn_key")).toString(),
                         root.value(QStringLiteral("awg_ip")).toString());
    });
}

bool NvoApiController::handleDeepLink(const QString &url)
{
    const QUrl u(url);
    if (u.scheme() != QStringLiteral("nvovpn")) {
        return false;
    }
    // nvovpn://login?code=XXXX
    const bool isLogin = (u.host() == QStringLiteral("login")) || u.path().contains(QStringLiteral("login"));
    if (!isLogin) {
        return false;
    }
    const QString code = QUrlQuery(u).queryItemValue(QStringLiteral("code"));
    if (code.isEmpty()) {
        return false;
    }
    loginByCode(code);
    return true;
}

void NvoApiController::applyUser(const QJsonObject &root)
{
    const QJsonObject user = root.value(QStringLiteral("user")).toObject();
    if (!user.isEmpty()) {
        m_userName = user.value(QStringLiteral("name")).toString();
        m_userEmail = user.value(QStringLiteral("email")).toString();
        m_hasSubscription = user.value(QStringLiteral("has_subscription")).toBool();
        emit userChanged();
    }

    if (root.contains(QStringLiteral("subscription"))) {
        const QJsonObject sub = root.value(QStringLiteral("subscription")).toObject();
        m_subPlan = sub.value(QStringLiteral("plan_name")).toString();
        m_subStatus = sub.value(QStringLiteral("status")).toString();
        m_subExpiresAt = sub.value(QStringLiteral("expires_at")).toString();
        if (m_subStatus == QStringLiteral("active")) {
            m_hasSubscription = true;
        }

        // Дней до конца подписки. Формат: 2026-07-12T20:27:23.000000Z — берём дату (первые 10 символов).
        const QDate expDate = QDate::fromString(m_subExpiresAt.left(10), QStringLiteral("yyyy-MM-dd"));
        if (expDate.isValid()) {
            const int days = static_cast<int>(QDate::currentDate().daysTo(expDate));
            m_subDaysRemaining = days > 0 ? days : 0;
        } else {
            m_subDaysRemaining = 0;
        }
        emit subscriptionChanged();
    }
}

QString NvoApiController::humanError(QNetworkReply *reply) const
{
    // Человеческие тексты ошибок (ТЗ §12.6), без технического жаргона.
    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::ConnectionRefusedError:
        return tr("Нет интернета — проверьте подключение");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return tr("Сервер не отвечает, попробуйте ещё раз");
    default:
        return tr("Что-то пошло не так, попробуйте ещё раз");
    }
}
