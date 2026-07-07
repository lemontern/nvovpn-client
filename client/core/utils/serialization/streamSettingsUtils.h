#ifndef STREAMSETTINGSUTILS_H
#define STREAMSETTINGSUTILS_H

#include <QJsonObject>
#include <QUrlQuery>

namespace amnezia::serialization::stream_utils
{

// Parses transport type-wise settings (kcp, http, ws, quic, grpc) from a URL
// query into a QJsonObject representing stream settings.
// Shared by vless and trojan Deserialize functions.
void parseTransportSettings(const QUrlQuery &query, const QString &type, QJsonObject &stream);

// Parses TLS/security settings (security, sni, alpn, reality) from a URL
// query into a QJsonObject representing stream settings.
// Shared by vless and trojan Deserialize functions.
void parseSecuritySettings(const QUrlQuery &query, QJsonObject &stream);

} // namespace amnezia::serialization::stream_utils

#endif // STREAMSETTINGSUTILS_H
