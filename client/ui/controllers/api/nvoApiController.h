#ifndef NVOAPICONTROLLER_H
#define NVOAPICONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QNetworkRequest>

class QNetworkAccessManager;
class QNetworkReply;
class QJsonObject;
class QTimer;
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
    Q_PROPERTY(QString balanceFormatted READ balanceFormatted NOTIFY userChanged)
    Q_PROPERTY(QString subscriptionPlan READ subscriptionPlan NOTIFY subscriptionChanged)
    Q_PROPERTY(QString subscriptionStatus READ subscriptionStatus NOTIFY subscriptionChanged)
    Q_PROPERTY(QString subscriptionExpiresAt READ subscriptionExpiresAt NOTIFY subscriptionChanged)
    Q_PROPERTY(int subscriptionDaysRemaining READ subscriptionDaysRemaining NOTIFY subscriptionChanged)
    Q_PROPERTY(int selectedServerId READ selectedServerId WRITE setSelectedServerId NOTIFY selectedServerChanged)
    Q_PROPERTY(bool onboardingDone READ onboardingDone NOTIFY onboardingChanged)
    Q_PROPERTY(QStringList favoriteCountries READ favoriteCountries NOTIFY favoritesChanged)

public:
    explicit NvoApiController(SecureQSettings *settings, NvoServersModel *serversModel, QObject *parent = nullptr);

    bool isAuthenticated() const;
    bool isBusy() const;
    QString userName() const;
    QString userEmail() const;
    bool hasSubscription() const;
    double balance() const;
    QString balanceFormatted() const;
    QString subscriptionPlan() const;
    QString subscriptionStatus() const;
    QString subscriptionExpiresAt() const;
    int subscriptionDaysRemaining() const;
    int selectedServerId() const;               // -1 = Авто (лучший сервер)
    bool onboardingDone() const;                // показан ли обучающий экран (§12.8)
    QStringList favoriteCountries() const;      // коды избранных стран (для подсветки/сортировки в UI)

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
    void loginWithGoogle();                      // Google-вход через polling: открыть браузер + опрашивать /auth/poll
    void loginWithApple();                       // Sign in with Apple через тот же polling-механизм (/app/login/apple)
    void openWebCabinet(const QString &redirect); // SSO в веб-ЛК: POST /auth/web-login → открыть url ("billing"/"plans"/"")
    void redeemPromo(const QString &code);        // POST /promo/redeem — активация промокода (кросс-промо 5 дней)
    void toggleFavoriteCountry(const QString &countryCode);  // добавить/убрать страну из избранного (сохраняется)
    void registerSuccessfulConnection();  // счётчик успешных коннектов; на 3-м — In-App Review (Android)

signals:
    void authenticationChanged();
    void busyChanged();
    void userChanged();
    void subscriptionChanged();
    void selectedServerChanged();
    void onboardingChanged();
    void favoritesChanged();

    void loginSucceeded();
    void loginFailed(const QString &message);
    void serversUpdated();
    void configReady(const QString &config, int serverId, const QString &serverName,
                     const QString &vpnKey, const QString &awgIp);
    void subscriptionRequired(const QString &message, const QString &reason);  // 403 — отказ /connect (email_unverified|ip_used|trial_used|no_plan|no_subscription)
    void sessionExpired();                      // 401 — токен отозван/истёк (вход на другом устройстве)
    void errorOccurred(const QString &message);

    // Промокод (/promo/redeem): granted → успех; иначе показываем готовый message бэкенда
    // (reason: already_active | code_invalid | code_used | code_empty | email_unverified | disposable).
    void promoSucceeded(const QString &message, int trialDays);
    void promoFailed(const QString &message, const QString &reason);

private:
    QNetworkRequest makeRequest(const QString &path, bool auth) const;
    void tryNextFailover();                     // следующий сервер в режиме Авто
    void setBusy(bool busy);
    void setToken(const QString &token);
    void applyUser(const QJsonObject &root);
    QString humanError(QNetworkReply *reply) const;
    void pollGoogleLogin();                      // один тик опроса /auth/poll?ds=...
    void stopGooglePolling();

    QNetworkAccessManager *m_nam;
    SecureQSettings *m_settings;
    NvoServersModel *m_serversModel;

    QString m_token;
    QString m_userName;
    QString m_userEmail;
    bool m_hasSubscription = false;
    double m_balance = 0.0;
    QString m_balanceFormatted;
    QString m_subPlan;
    QString m_subStatus;
    QString m_subExpiresAt;
    int m_subDaysRemaining = 0;
    int m_selectedServerId = -1;
    bool m_busy = false;
    bool m_onboardingDone = false;
    QStringList m_favoriteCountries;

    // Авто-failover (ТЗ §12.6): в режиме «Авто» перебираем рабочие ноды молча.
    QList<int> m_failoverQueue;
    bool m_inFailover = false;

    // Google-вход через polling (без deep-link): открываем браузер, опрашиваем /auth/poll по ds.
    QTimer *m_googlePollTimer = nullptr;
    QString m_googleDs;
    int m_googlePollElapsedMs = 0;
};

#endif // NVOAPICONTROLLER_H
