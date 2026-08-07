#ifndef IOS_CONTROLLER_H
#define IOS_CONTROLLER_H

#include "core/protocols/vpnProtocol.h"
#include <functional>
#include <QVariant>
#include <QVariantMap>
#include <QStringList>
#include <QList>
#include <QElapsedTimer>
#include <QTimer>
#include <atomic>

#ifdef __OBJC__
    #import <Foundation/Foundation.h>
@class NETunnelProviderManager;
#endif

using namespace amnezia;

struct Action
{
    static const char *start;
    static const char *restart;
    static const char *stop;
    static const char *getTunnelId;
    static const char *getStatus;
};

struct MessageKey
{
    static const char *action;
    static const char *tunnelId;
    static const char *config;
    static const char *errorCode;
    static const char *host;
    static const char *port;
    static const char *isOnDemand;
    static const char *SplitTunnelType;
    static const char *SplitTunnelSites;
};

class IosController : public QObject
{
    Q_OBJECT

public:
    static IosController *Instance();

    virtual ~IosController() override = default;

    bool initialize();
    bool connectVpn(amnezia::Proto proto, const QJsonObject &configuration);
    void disconnectVpn();

    void vpnStatusDidChange(void *pNotification);
    
    void vpnConfigurationDidChange(void *pNotification);

    void getBackendLogs(std::function<void(const QString &)> &&callback);
    void checkStatus();
    // Слежение за уже поднятым туннелем: см. комментарии в .mm.
    void startLivenessWatch(uint64_t rxBytes);
    void stopLivenessWatch();
    void checkTunnelLiveness(uint64_t rxBytes, long long lastHandshakeSec);

    bool shareText(const QStringList &filesToSend);
    QString openFile();

    void purchaseProduct(const QString &productId,
                         std::function<void(bool success,
                                            const QString &transactionId,
                                            const QString &purchasedProductId,
                                            const QString &originalTransactionId,
                                            const QString &errorString)> &&callback);
    void restorePurchases(std::function<void(bool success,
                                             const QList<QVariantMap> &transactions,
                                             const QString &errorString)> &&callback);

    // Fetch product info for given product identifiers and return basic fields for logging
    void fetchProducts(const QStringList &productIds,
                       std::function<void(const QList<QVariantMap> &products,
                                          const QStringList &invalidIds,
                                          const QString &errorString)> &&callback);

    void requestInetAccess();
    bool isTestFlight();
signals:
    void connectionStateChanged(Vpn::ConnectionState state);
    void bytesChanged(quint64 receivedBytes, quint64 sentBytes);
    // NvoVPN macOS: расширение требует одобрения юзером в System Settings (один раз).
    void systemExtensionNeedsApproval();
    void importConfigFromOutside(const QString);
    void importBackupFromOutside(const QString);

    void finished();

protected slots:

private:
    explicit IosController();

    bool setupOpenVPN();
    bool setupWireGuard();
    bool setupAwg();
    bool setupXray();
    bool setupSSXray();

    bool startOpenVPN(const QString &config);
    bool startWireGuard(const QString &jsonConfig);
    bool startXray(const QString &jsonConfig);

    void startTunnel();
    void emitConnectionStateIfChanged(Vpn::ConnectionState state);

    // NvoVPN macOS: активация Network Extension как System Extension (Developer ID, вне App Store).
    // На iOS/App Store расширение — App Extension и активации не требует; на macOS Developer ID
    // нужно явно активировать через OSSystemExtensionRequest (юзер одобряет в System Settings),
    // иначе провайдер com.nvovpn.app не зарегистрирован → туннель не стартует ("Подключаем…" висит).
    // Возвращает true если sysext уже активен (можно стартовать туннель), false если ждём одобрения.
    bool ensureSystemExtensionActivated();

private:
    void *m_iosControllerWrapper {};
#ifdef __OBJC__
    NETunnelProviderManager *m_currentTunnel {};
    NSString *m_serverAddress {};
    bool isOurManager(NETunnelProviderManager *manager);
    void sendVpnExtensionMessage(NSDictionary *message, std::function<void(NSDictionary *)> callback = nullptr);
#endif

    amnezia::Proto m_proto;
    QJsonObject m_rawConfig;
    QString m_tunnelId;
    uint64_t m_txBytes = 0;
    uint64_t m_rxBytes = 0;
    bool m_handshakeAwaiting = false;
    bool m_handshakeConfirmed = false;
    QElapsedTimer m_handshakeTimer;
    // Контроль живости УЖЕ поднятого туннеля. Проверка выше сторожит только первое рукопожатие
    // и после него замолкает, поэтому обрыв посреди сессии оставался незамеченным: NE держит
    // туннель, дефолтный маршрут в нём, а трафик уже режется. Своего опроса тут нет —
    // checkStatus() дёргается по NEVPNStatusDidChange, то есть только когда статус меняется,
    // а при обрыве он как раз не меняется. Поэтому заводим собственный таймер.
    QTimer *m_livenessTimer = nullptr;
    // rx на прошлой проверке: если растёт, туннель живой независимо от возраста рукопожатия.
    uint64_t m_livenessRxMark = 0;
    Vpn::ConnectionState m_lastEmittedState = Vpn::ConnectionState::Unknown;
    std::atomic_bool m_statusRequestInFlight { false };
};

#endif // IOS_CONTROLLER_H
