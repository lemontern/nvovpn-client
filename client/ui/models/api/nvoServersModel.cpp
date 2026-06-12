#include "nvoServersModel.h"

#include <QJsonObject>

#include "logger.h"

namespace
{
    Logger logger("NvoServersModel");
}

NvoServersModel::NvoServersModel(QObject *parent) : QAbstractListModel(parent)
{
}

int NvoServersModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_servers.size();
}

QVariant NvoServersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_servers.size())
        return QVariant();

    const ServerInfo &s = m_servers.at(index.row());

    switch (role) {
    case ServerIdRole: return s.id;
    case NameRole: return s.name;
    case CountryCodeRole: return s.countryCode;
    case CountryImageCodeRole: return s.countryCode.toUpper();
    case CityRole: return s.city;
    case RecommendedRole: return s.recommended;
    case LoadPercentRole: return s.loadPercent;
    case OnlineRole: return s.online;
    }

    return QVariant();
}

void NvoServersModel::updateModel(const QJsonArray &servers)
{
    beginResetModel();

    m_servers.clear();
    for (const QJsonValue &value : servers) {
        const QJsonObject obj = value.toObject();

        ServerInfo info;
        info.id = obj.value(QStringLiteral("id")).toInt(-1);
        info.name = obj.value(QStringLiteral("name")).toString().trimmed();
        info.countryCode = obj.value(QStringLiteral("country_code")).toString();
        info.city = obj.value(QStringLiteral("location")).toString();
        info.recommended = obj.value(QStringLiteral("is_recommended")).toBool();
        info.loadPercent = obj.value(QStringLiteral("load_percent")).toInt(0);

        const QString status = obj.value(QStringLiteral("status")).toString();
        const QString health = obj.value(QStringLiteral("health_status")).toString();
        // Сервер считаем доступным, если активен и здоров (на /servers нет булева online).
        info.online = (status.isEmpty() || status == QStringLiteral("active"))
                && (health.isEmpty() || health == QStringLiteral("healthy"));

        if (info.id >= 0)
            m_servers.push_back(info);
    }

    endResetModel();
}

int NvoServersModel::count() const
{
    return m_servers.size();
}

int NvoServersModel::serverIdAt(int index) const
{
    if (index < 0 || index >= m_servers.size())
        return -1;
    return m_servers.at(index).id;
}

QString NvoServersModel::countryCodeAt(int index) const
{
    if (index < 0 || index >= m_servers.size())
        return QString();
    return m_servers.at(index).countryCode;
}

QString NvoServersModel::nameAt(int index) const
{
    if (index < 0 || index >= m_servers.size())
        return QString();
    return m_servers.at(index).name;
}

int NvoServersModel::recommendedServerId() const
{
    for (const ServerInfo &s : m_servers) {
        if (s.recommended)
            return s.id;
    }
    return -1;
}

int NvoServersModel::bestServerId() const
{
    // 1) рекомендованный и online
    for (const ServerInfo &s : m_servers) {
        if (s.recommended && s.online)
            return s.id;
    }
    // 2) наименее загруженный online
    int bestId = -1;
    int bestLoad = 101;
    for (const ServerInfo &s : m_servers) {
        if (s.online && s.loadPercent < bestLoad) {
            bestLoad = s.loadPercent;
            bestId = s.id;
        }
    }
    if (bestId != -1)
        return bestId;
    // 3) хоть что-то
    return m_servers.isEmpty() ? -1 : m_servers.first().id;
}

int NvoServersModel::indexOfServerId(int serverId) const
{
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers.at(i).id == serverId)
            return i;
    }
    return -1;
}

QHash<int, QByteArray> NvoServersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ServerIdRole] = "serverId";
    roles[NameRole] = "name";
    roles[CountryCodeRole] = "countryCode";
    roles[CountryImageCodeRole] = "countryImageCode";
    roles[CityRole] = "city";
    roles[RecommendedRole] = "recommended";
    roles[LoadPercentRole] = "loadPercent";
    roles[OnlineRole] = "online";
    return roles;
}
