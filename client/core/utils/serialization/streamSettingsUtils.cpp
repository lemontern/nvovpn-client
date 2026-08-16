#include "streamSettingsUtils.h"

#include <QJsonArray>
#include <QStringList>
#include <QUrl>
#include "3rd/QJsonStruct/QJsonIO.hpp"

namespace amnezia::serialization::stream_utils
{

void parseTransportSettings(const QUrlQuery &query, const QString &type, QJsonObject &stream)
{
    if (type != "tcp")
        QJsonIO::SetValue(stream, type, "network");

    if (type == "kcp")
    {
        const auto hasSeed = query.hasQueryItem("seed");
        if (hasSeed)
            QJsonIO::SetValue(stream, query.queryItemValue("seed"), { "kcpSettings", "seed" });

        const auto hasHeaderType = query.hasQueryItem("headerType");
        const auto headerType = hasHeaderType ? query.queryItemValue("headerType") : "none";
        if (headerType != "none")
            QJsonIO::SetValue(stream, headerType, { "kcpSettings", "header", "type" });
    }
    else if (type == "http")
    {
        const auto hasPath = query.hasQueryItem("path");
        const auto path = hasPath ? QUrl::fromPercentEncoding(query.queryItemValue("path").toUtf8()) : "/";
        if (path != "/")
            QJsonIO::SetValue(stream, path, { "httpSettings", "path" });

        const auto hasHost = query.hasQueryItem("host");
        if (hasHost)
        {
            const auto hosts = QJsonArray::fromStringList(query.queryItemValue("host").split(","));
            QJsonIO::SetValue(stream, hosts, { "httpSettings", "host" });
        }
    }
    else if (type == "ws")
    {
        const auto hasPath = query.hasQueryItem("path");
        const auto path = hasPath ? QUrl::fromPercentEncoding(query.queryItemValue("path").toUtf8()) : "/";
        if (path != "/")
            QJsonIO::SetValue(stream, path, { "wsSettings", "path" });

        const auto hasHost = query.hasQueryItem("host");
        if (hasHost)
        {
            QJsonIO::SetValue(stream, query.queryItemValue("host"), { "wsSettings", "headers", "Host" });
        }
    }
    else if (type == "quic")
    {
        const auto hasQuicSecurity = query.hasQueryItem("quicSecurity");
        if (hasQuicSecurity)
        {
            const auto quicSecurity = query.queryItemValue("quicSecurity");
            QJsonIO::SetValue(stream, quicSecurity, { "quicSettings", "security" });

            if (quicSecurity != "none")
            {
                const auto key = query.queryItemValue("key");
                QJsonIO::SetValue(stream, key, { "quicSettings", "key" });
            }

            const auto hasHeaderType = query.hasQueryItem("headerType");
            const auto headerType = hasHeaderType ? query.queryItemValue("headerType") : "none";
            if (headerType != "none")
                QJsonIO::SetValue(stream, headerType, { "quicSettings", "header", "type" });
        }
    }
    else if (type == "grpc")
    {
        const auto hasServiceName = query.hasQueryItem("serviceName");
        if (hasServiceName)
        {
            const auto serviceName = QUrl::fromPercentEncoding(query.queryItemValue("serviceName").toUtf8());
            QJsonIO::SetValue(stream, serviceName, { "grpcSettings", "serviceName" });
        }

        const auto hasMode = query.hasQueryItem("mode");
        if (hasMode)
        {
            const auto multiMode = QUrl::fromPercentEncoding(query.queryItemValue("mode").toUtf8()) == "multi";
            QJsonIO::SetValue(stream, multiMode, { "grpcSettings", "multiMode" });
        }
    }
}

void parseSecuritySettings(const QUrlQuery &query, QJsonObject &stream)
{
    const auto hasSecurity = query.hasQueryItem("security");
    const auto security = hasSecurity ? query.queryItemValue("security") : "none";
    const auto tlsKey = security == "xtls" ? "xtlsSettings" : ( security == "tls" ? "tlsSettings" : "realitySettings" );
    if (security != "none")
    {
        QJsonIO::SetValue(stream, security, "security");
    }
    // sni
    const auto hasSNI = query.hasQueryItem("sni");
    if (hasSNI)
    {
        const auto sni = query.queryItemValue("sni");
        QJsonIO::SetValue(stream, sni, { tlsKey, "serverName" });
    }
    // alpn
    const auto hasALPN = query.hasQueryItem("alpn");
    if (hasALPN)
    {
        const auto alpnRaw = QUrl::fromPercentEncoding(query.queryItemValue("alpn").toUtf8());
        QStringList aplnElems = alpnRaw.split(",");
        aplnElems.removeAll("h2");
        if (!aplnElems.isEmpty()) {
            const auto alpnArray = QJsonArray::fromStringList(aplnElems);
            QJsonIO::SetValue(stream, alpnArray, { tlsKey, "alpn" });
        }
    }

    if (security == "reality")
    {
        if (query.hasQueryItem("fp"))
        {
            const auto fp = QUrl::fromPercentEncoding(query.queryItemValue("fp").toUtf8());
            QJsonIO::SetValue(stream, fp, { "realitySettings", "fingerprint" });
        }
        if (query.hasQueryItem("pbk"))
        {
            const auto pbk = QUrl::fromPercentEncoding(query.queryItemValue("pbk").toUtf8());
            QJsonIO::SetValue(stream, pbk, { "realitySettings", "publicKey" });
        }
        if (query.hasQueryItem("spiderX"))
        {
            const auto spiderX = QUrl::fromPercentEncoding(query.queryItemValue("spiderX").toUtf8());
            QJsonIO::SetValue(stream, spiderX, { "realitySettings", "spiderX" });
        }
        if (query.hasQueryItem("sid"))
        {
            const auto sid = QUrl::fromPercentEncoding(query.queryItemValue("sid").toUtf8());
            QJsonIO::SetValue(stream, sid, { "realitySettings", "shortId" });
        }
    }
}

} // namespace amnezia::serialization::stream_utils
