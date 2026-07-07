// Copyright (c) Qv2ray, A Qt frontend for V2Ray. Written in C++.
// This file is part of the Qv2ray VPN client.
//
// Qv2ray, A Qt frontend for V2Ray. Written in C++

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Copyright (c) 2024 AmneziaVPN
// This file has been modified for AmneziaVPN
//
// This file is based on the work of the Qv2ray VPN client.
// The original code of the Qv2ray, A Qt frontend for V2Ray. Written in C++ and licensed under GPL3.
//
// The modified version of this file is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this file. If not, see <https://www.gnu.org/licenses/>.


#include "3rd/QJsonStruct/QJsonIO.hpp"
#include <QUrlQuery>
#include "serialization.h"
#include "streamSettingsUtils.h"

namespace amnezia::serialization::vless
{
QJsonObject Deserialize(const QString &str, QString *alias, QString *errMessage)
{
    // must start with vless://
    if (!str.startsWith("vless://"))
    {
        *errMessage = QObject::tr("VLESS link should start with vless://");
        return QJsonObject();
    }

    // parse url
    QUrl url(str);
    if (!url.isValid())
    {
        *errMessage = QObject::tr("link parse failed: %1").arg(url.errorString());
        return QJsonObject();
    }

    // fetch host
    const auto hostRaw = url.host();
    if (hostRaw.isEmpty())
    {
        *errMessage = QObject::tr("empty host");
        return QJsonObject();
    }
    const auto host = (hostRaw.startsWith('[') && hostRaw.endsWith(']')) ? hostRaw.mid(1, hostRaw.length() - 2) : hostRaw;

    // fetch port
    const auto port = url.port();
    if (port == -1)
    {
        *errMessage = QObject::tr("missing port");
        return QJsonObject();
    }

    // fetch remarks
    const auto remarks = url.fragment();
    if (!remarks.isEmpty())
    {
        *alias = remarks;
    }

    // fetch uuid
    const auto uuid = url.userInfo();
    if (uuid.isEmpty())
    {
        *errMessage = QObject::tr("missing uuid");
        return QJsonObject();
    }

    // initialize QJsonObject with basic info
    QJsonObject outbound;
    QJsonObject stream;

    QJsonIO::SetValue(outbound, "vless", "protocol");
    QJsonIO::SetValue(outbound, host, { "settings", "vnext", 0, "address" });
    QJsonIO::SetValue(outbound, port, { "settings", "vnext", 0, "port" });
    QJsonIO::SetValue(outbound, uuid, { "settings", "vnext", 0, "users", 0, "id" });

    // parse query
    QUrlQuery query(url.query());

    // handle type
    const auto hasType = query.hasQueryItem("type");
    const auto type = hasType ? query.queryItemValue("type") : "tcp";

    // handle encryption
    const auto hasEncryption = query.hasQueryItem("encryption");
    const auto encryption = hasEncryption ? query.queryItemValue("encryption") : "none";
    QJsonIO::SetValue(outbound, encryption, { "settings", "vnext", 0, "users", 0, "encryption" });

    stream_utils::parseTransportSettings(query, type, stream);
    stream_utils::parseSecuritySettings(query, stream);

    // xtls-specific
    const auto security = query.hasQueryItem("security") ? query.queryItemValue("security") : "none";
    if (security == "xtls" || security == "reality")
    {
        const auto flow = query.queryItemValue("flow");
        QJsonIO::SetValue(outbound, flow, { "settings", "vnext", 0, "users", 0, "flow" });
    }

    // assembling config
    QJsonObject root;
    outbound["streamSettings"] = stream;
    QJsonObject inbound = inbounds::GenerateInboundEntry();
    root["outbounds"] = QJsonArray{ outbound };
    root["inbounds"] = QJsonArray { inbound };
    return root;
}

const QString Serialize(const VlessServerObject &server, const QString &alias)
{
    
    QUrl url;
    
    // Set basic URL components
    url.setScheme("vless");
    url.setUserInfo(server.id);
    url.setHost(server.address);
    url.setPort(server.port);

    QUrlQuery query;

    if (!server.network.isEmpty() && server.network != "tcp") {
        query.addQueryItem("type", server.network);
    }
    
    if (!server.encryption.isEmpty()) {
        query.addQueryItem("encryption", server.encryption);
    }
    
    if (!server.security.isEmpty() && server.security != "none") {
        query.addQueryItem("security", server.security);
    }
    
    if (!server.flow.isEmpty() && (server.security == "xtls" || server.security == "reality")) {
        query.addQueryItem("flow", server.flow);
    }
    
    if (!server.serverName.isEmpty()) {
        query.addQueryItem("sni", server.serverName);
    }
    
    if (server.security == "reality") {
        if (!server.fingerprint.isEmpty()) {
            query.addQueryItem("fp", server.fingerprint);
        }
        
        if (!server.publicKey.isEmpty()) {
            query.addQueryItem("pbk", server.publicKey);
        }
        
        if (!server.shortId.isEmpty()) {
            query.addQueryItem("sid", server.shortId);
        }
        
        if (!server.spiderX.isEmpty()) {
            query.addQueryItem("spiderX", server.spiderX);
        }
    }
    
    url.setQuery(query);
    
    if (!alias.isEmpty()) {
        url.setFragment(alias);
    }
    
    return url.toString(QUrl::ComponentFormattingOption::FullyEncoded);
}
}

