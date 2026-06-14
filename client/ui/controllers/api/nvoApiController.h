#ifndef NVOAPICONTROLLER_H
#define NVOAPICONTROLLER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkRequest>

class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;
class SecureQSettings;
class NvoServersModel;

// API-клиент NvoVPN (https://nvovpn.com/api/v1). Обычный HTTPS + Bearer-токен.
// НЕ использует gateway Amnezia (RSA+AES). Отдаёт сигналы/наполняет модели для QML.
// Готовый awg .conf приходит из POST /connect (сигнал configReady) — коннект оркеструется в CoreController.
class NvoApiController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isAuthenticated READ isAuthenticated NOTIFY authenticationChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY userChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY userChanged)
    Q_PROPERTY(bool hasSubscription READ hasSubscription NOTIFY subscriptionChanged)
    Q_PROPERTY(double balance READ balance NOTIFY userChanged)
    Q_PROPERTY(QString subscriptionPlan READ subscriptionPlan NOTIFY subscriptionChanged)
    Q_PROPERTY(QString subscriptionStatus READ subscriptionStatus NOTIFY subscriptionChanged)
    Q_PROPERTY(QString subscriptionExpiresAt READ subscriptionExpiresAt NOTIFY subscriptionChanged)
    Q_PROPERTY(int subscriptionDaysRemaining READ subscriptionDaysRemaining NOTIFY subscriptionChanged)
    Q_PROPERTY(int selectedServerId READ selectedServerId WRITE setSelectedServerId NOTIFY selectedServerChanged)
    Q_PROPERTY(bool onboardingDone READ onboardingDone NOTIFY onboardingChanged)

public:
    explicit NvoApiController(SecureQSettings *settings, NvoServersModel *serversModel, QObject *parent = nullptr);

    bool isAuthenticated() const;
    bool isBusy() const;
    QString userName() const;
    QString userEmail() const;
    bool hasSubscription() const;
    double balance() const;
    QString subscriptionPlan() const;
    QString subscriptionStatus() const;
    QString subscriptionExpiresAt() const;
    int subscriptionDaysRemaining() const;
    int selectedServerId() const;               // -1 = Авто (лучший сервер)
    bool onboardingDone() const;                // показан ли обучающий экран (§12.8)

public slots:
    void setOnboardingDone();
    void login(const QString &email, const QString &password);
    void loginByCode(const QString &code);
    void logout();
    void refreshServers();
    void refreshUser();
    void requestConfig(int serverId);          // POST /connect → сигнал configReady
    void connectToSelected();                   // выбранный сервер или Авто → requestConfig
    void setSelectedServerId(int serverId);
    bool handleDeepLink(const QString &url);    // nvovpn://login?code=XXXX → loginByCode
    QString token() const;

signals:
    void authenticationChanged();
    void busyChanged();
    void userChanged();
    void subscriptionChanged();
    void selectedServerChanged();
    void onboardingChanged();

    void loginSucceeded();
    void loginFailed(const QString &message);
    void serversUpdated();
    void configReady(const QString &config, int serverId, const QString &serverName,
                     const QString &vpnKey, const QString &awgIp);
    void subscriptionRequired();                // 403 — нет активной подписки
    void sessionExpired();                      // 401 — токен отозван/истёк (вход на другом устройстве)
    void errorOccurred(const QString &message);

private:
    QNetworkRequest makeRequest(const QString &path, bool auth) const;
    void tryNextFailover();                     // следующий сервер в режиме Авто
    void setBusy(bool busy);
    void setToken(const QString &token);
    void applyUser(const QJsonObject &root);
    QString humanError(QNetworkReply *reply) const;

    QNetworkAccessManager *m_nam;
    SecureQSettings *m_settings;
    NvoServersModel *m_serversModel;

    QString m_token;
    QString m_userName;
    QString m_userEmail;
    bool m_hasSubscription = false;
    double m_balance = 0.0;
    QString m_subPlan;
    QString m_subStatus;
    QString m_subExpiresAt;
    int m_subDaysRemaining = 0;
    int m_selectedServerId = -1;
    bool m_busy = false;
    bool m_onboardingDone = false;

    // Авто-failover (ТЗ §12.6): в режиме «Авто» перебираем рабочие ноды молча.
    QList<int> m_failoverQueue;
    bool m_inFailover = false;
};

#endif // NVOAPICONTROLLER_H
