#ifndef NVOSERVERSMODEL_H
#define NVOSERVERSMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QVector>

// Список VPN-серверов (стран) NvoVPN из GET /servers.
// Только человеческие данные для UI: страна, город, флаг, нагрузка. Без ключей/endpoint'ов.
class NvoServersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ServerIdRole = Qt::UserRole + 1,
        NameRole,
        CountryCodeRole,
        CountryImageCodeRole,
        CityRole,
        RecommendedRole,
        LoadPercentRole,
        OnlineRole
    };

    explicit NvoServersModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void updateModel(const QJsonArray &servers);
    int count() const;
    int serverIdAt(int index) const;       // -1 если индекс невалидный
    QString countryCodeAt(int index) const;
    QString nameAt(int index) const;
    int recommendedServerId() const;        // -1 если нет рекомендованного
    int bestServerId() const;               // рекомендованный online; иначе наименее загруженный online; иначе первый
    int indexOfServerId(int serverId) const;

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    struct ServerInfo {
        int id = -1;
        QString name;
        QString countryCode;
        QString city;
        bool recommended = false;
        int loadPercent = 0;
        bool online = true;
    };

    QVector<ServerInfo> m_servers;
};

#endif // NVOSERVERSMODEL_H
