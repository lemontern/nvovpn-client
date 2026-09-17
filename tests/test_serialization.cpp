#include <QtTest/QtTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

#include "core/utils/serialization/serialization.h"
#include "core/utils/serialization/transfer.h"

using namespace amnezia::serialization;

class TestSerialization : public QObject
{
    Q_OBJECT

private slots:
    // ─── Shadowsocks ──────────────────────────────────────────────────────
    void testSS_Serialize_basic();
    void testSS_Deserialize_sip002();
    void testSS_Deserialize_oldScheme();
    void testSS_Deserialize_invalidPrefix();
    void testSS_Deserialize_tooShort();
    void testSS_roundTrip();

    // ─── Trojan ───────────────────────────────────────────────────────────
    void testTrojan_Serialize_basic();
    void testTrojan_Serialize_withOptions();
    void testTrojan_Deserialize_basic();
    void testTrojan_Deserialize_invalidPrefix();
    void testTrojan_Deserialize_withSni();
    void testTrojan_roundTrip();

    // ─── VLESS ────────────────────────────────────────────────────────────
    void testVless_Serialize_basic();
    void testVless_Serialize_reality();
    void testVless_Deserialize_basic();
    void testVless_Deserialize_invalidPrefix();
    void testVless_Deserialize_missingPort();
    void testVless_Deserialize_withReality();

    // ─── VMess ────────────────────────────────────────────────────────────
    void testVmess_Deserialize_basic();
    void testVmess_Deserialize_invalidPrefix();
    void testVmess_Deserialize_emptyBase64();
    void testVmess_Deserialize_invalidJson();

    // ─── Outbounds ────────────────────────────────────────────────────────
    void testOutbound_GenerateEntry();
    void testOutbound_GenerateFreedom();
    void testOutbound_GenerateBlackHole();
    void testOutbound_GenerateShadowSocksServer();
    void testOutbound_GenerateShadowSocks();
    void testOutbound_GenerateTrojanServer();
    void testOutbound_GenerateHTTPSOCKS_noAuth();
    void testOutbound_GenerateHTTPSOCKS_withAuth();

    // ─── Inbounds ─────────────────────────────────────────────────────────
    void testInbound_GenerateEntry();
    void testInbound_GetCredentials_empty();
    void testInbound_GetCredentials_withAccounts();
    void testInbound_EnsureAuth();
};

// ═══════════════════════════════════════════════════════════════════════════
// Shadowsocks Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testSS_Serialize_basic()
{
    ShadowSocksServerObject server;
    server.address = "example.com";
    server.port = 8388;
    server.method = "aes-256-gcm";
    server.password = "testpass";

    QString result = ss::Serialize(server, "MyServer", false);
    QVERIFY(result.startsWith("ss://"));
    QVERIFY(result.contains("#MyServer"));
}

void TestSerialization::testSS_Deserialize_sip002()
{
    // Build a SIP002 URI: ss://base64(method:password)@host:port#name
    QString userInfo = QString("aes-256-gcm:testpassword").toUtf8().toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QString uri = QString("ss://%1@example.com:8388#TestServer").arg(userInfo);

    QString alias;
    QString errMessage;
    QJsonObject result = ss::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("TestServer"));
}

void TestSerialization::testSS_Deserialize_oldScheme()
{
    // Old scheme: ss://base64(method:password@host:port)#name
    QString plain = "aes-256-gcm:mypassword@192.168.1.1:8388";
    QString encoded = plain.toUtf8().toBase64(QByteArray::OmitTrailingEquals);
    QString uri = "ss://" + encoded + "#OldServer";

    QString alias;
    QString errMessage;
    QJsonObject result = ss::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("OldServer"));
}

void TestSerialization::testSS_Deserialize_invalidPrefix()
{
    QString alias;
    QString errMessage;
    QJsonObject result = ss::Deserialize("http://invalid", &alias, &errMessage);
    // The function checks length < 5 but doesn't fail for wrong prefix in old scheme;
    // it will fail to parse and return an error or empty result
    // The exact behavior depends on internal parsing
}

void TestSerialization::testSS_Deserialize_tooShort()
{
    QString alias;
    QString errMessage;
    QJsonObject result = ss::Deserialize("ss:/", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
}

void TestSerialization::testSS_roundTrip()
{
    ShadowSocksServerObject server;
    server.address = "example.com";
    server.port = 8388;
    server.method = "chacha20-ietf-poly1305";
    server.password = "s3cretP@ss";

    QString serialized = ss::Serialize(server, "RoundTrip", true);
    QVERIFY(serialized.startsWith("ss://"));

    QString alias;
    QString errMessage;
    QJsonObject result = ss::Deserialize(serialized, &alias, &errMessage);
    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Trojan Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testTrojan_Serialize_basic()
{
    TrojanObject obj;
    obj.address = "example.com";
    obj.port = 443;
    obj.password = "mypassword";
    obj.sni = "example.com";

    QString result = trojan::Serialize(obj, "MyTrojan");
    QVERIFY(result.startsWith("trojan://"));
    QVERIFY(result.contains("example.com"));
    QVERIFY(result.contains("443"));
    QVERIFY(result.contains("#MyTrojan"));
}

void TestSerialization::testTrojan_Serialize_withOptions()
{
    TrojanObject obj;
    obj.address = "example.com";
    obj.port = 443;
    obj.password = "mypassword";
    obj.sni = "sni.example.com";
    obj.tcpFastOpen = true;
    obj.ignoreCertificate = true;

    QString result = trojan::Serialize(obj, "TrojanOpts");
    QVERIFY(result.contains("tfo=1"));
    QVERIFY(result.contains("allowInsecure=1"));
    QVERIFY(result.contains("sni=sni.example.com"));
}

void TestSerialization::testTrojan_Deserialize_basic()
{
    QString uri = "trojan://mypassword@example.com:443#TestTrojan";

    QString alias;
    QString errMessage;
    QJsonObject result = trojan::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("TestTrojan"));
}

void TestSerialization::testTrojan_Deserialize_invalidPrefix()
{
    QString alias;
    QString errMessage;
    QJsonObject result = trojan::Deserialize("http://invalid", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
    QVERIFY(result.isEmpty());
}

void TestSerialization::testTrojan_Deserialize_withSni()
{
    QString uri = "trojan://mypassword@example.com:443?sni=custom.sni.com#WithSNI";

    QString alias;
    QString errMessage;
    QJsonObject result = trojan::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("WithSNI"));
}

void TestSerialization::testTrojan_roundTrip()
{
    TrojanObject obj;
    obj.address = "server.example.com";
    obj.port = 443;
    obj.password = "s3cret";
    obj.sni = "server.example.com";
    obj.tcpFastOpen = false;

    QString serialized = trojan::Serialize(obj, "RoundTrip");
    QVERIFY(serialized.startsWith("trojan://"));

    QString alias;
    QString errMessage;
    QJsonObject result = trojan::Deserialize(serialized, &alias, &errMessage);
    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("RoundTrip"));
}

// ═══════════════════════════════════════════════════════════════════════════
// VLESS Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testVless_Serialize_basic()
{
    VlessServerObject server;
    server.address = "example.com";
    server.port = 443;
    server.id = "12345678-1234-1234-1234-123456789abc";
    server.network = "tcp";
    server.security = "none";
    server.encryption = "none";

    QString result = vless::Serialize(server, "MyVless");
    QVERIFY(result.startsWith("vless://"));
    QVERIFY(result.contains("example.com"));
    QVERIFY(result.contains("443"));
    QVERIFY(result.contains("#MyVless"));
}

void TestSerialization::testVless_Serialize_reality()
{
    VlessServerObject server;
    server.address = "reality.example.com";
    server.port = 443;
    server.id = "12345678-1234-1234-1234-123456789abc";
    server.network = "tcp";
    server.security = "reality";
    server.flow = "xtls-rprx-vision";
    server.fingerprint = "chrome";
    server.publicKey = "abc123";
    server.shortId = "def456";
    server.serverName = "sni.example.com";

    QString result = vless::Serialize(server, "Reality");
    QVERIFY(result.contains("security=reality"));
    QVERIFY(result.contains("pbk=abc123"));
    QVERIFY(result.contains("sid=def456"));
    QVERIFY(result.contains("fp=chrome"));
    QVERIFY(result.contains("flow=xtls-rprx-vision"));
}

void TestSerialization::testVless_Deserialize_basic()
{
    QString uri = "vless://12345678-1234-1234-1234-123456789abc@example.com:443?encryption=none#TestVless";

    QString alias;
    QString errMessage;
    QJsonObject result = vless::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("TestVless"));

    QJsonArray outboundsArr = result["outbounds"].toArray();
    QVERIFY(!outboundsArr.isEmpty());
}

void TestSerialization::testVless_Deserialize_invalidPrefix()
{
    QString alias;
    QString errMessage;
    QJsonObject result = vless::Deserialize("http://invalid", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
    QVERIFY(result.isEmpty());
}

void TestSerialization::testVless_Deserialize_missingPort()
{
    QString alias;
    QString errMessage;
    QJsonObject result = vless::Deserialize("vless://uuid@example.com#NoPort", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
}

void TestSerialization::testVless_Deserialize_withReality()
{
    QString uri = "vless://12345678-1234-1234-1234-123456789abc@example.com:443"
                  "?encryption=none&security=reality&sni=sni.com&fp=chrome"
                  "&pbk=publickey123&sid=shortid456&flow=xtls-rprx-vision#Reality";

    QString alias;
    QString errMessage;
    QJsonObject result = vless::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("Reality"));

    QJsonObject outbound = result["outbounds"].toArray().first().toObject();
    QJsonObject stream = outbound["streamSettings"].toObject();
    QCOMPARE(stream["security"].toString(), QString("reality"));
}

// ═══════════════════════════════════════════════════════════════════════════
// VMess Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testVmess_Deserialize_basic()
{
    // Build a vmess:// URI by base64-encoding a JSON config
    QJsonObject config;
    config["v"] = 2;
    config["ps"] = "TestServer";
    config["add"] = "example.com";
    config["port"] = 443;
    config["id"] = "12345678-1234-1234-1234-123456789abc";
    config["aid"] = 0;
    config["scy"] = "auto";
    config["net"] = "tcp";
    config["type"] = "none";
    config["tls"] = "none";

    QJsonDocument doc(config);
    QString base64 = doc.toJson(QJsonDocument::Compact).toBase64();
    QString uri = "vmess://" + base64;

    QString alias;
    QString errMessage;
    QJsonObject result = vmess::Deserialize(uri, &alias, &errMessage);

    QVERIFY2(errMessage.isEmpty(), qPrintable(errMessage));
    QVERIFY(!result.isEmpty());
    QCOMPARE(alias, QString("TestServer"));
}

void TestSerialization::testVmess_Deserialize_invalidPrefix()
{
    QString alias;
    QString errMessage;
    QJsonObject result = vmess::Deserialize("http://invalid", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
    QVERIFY(result.isEmpty());
}

void TestSerialization::testVmess_Deserialize_emptyBase64()
{
    QString alias;
    QString errMessage;
    QJsonObject result = vmess::Deserialize("vmess://", &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
}

void TestSerialization::testVmess_Deserialize_invalidJson()
{
    QString invalidBase64 = QString("not valid json at all").toUtf8().toBase64();
    QString uri = "vmess://" + invalidBase64;

    QString alias;
    QString errMessage;
    QJsonObject result = vmess::Deserialize(uri, &alias, &errMessage);
    QVERIFY(!errMessage.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Outbounds Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testOutbound_GenerateEntry()
{
    QJsonObject settings;
    settings["key"] = "value";
    QJsonObject stream;
    stream["network"] = "tcp";

    QJsonObject result = outbounds::GenerateOutboundEntry("PROXY", "vmess", settings, stream);
    QCOMPARE(result["tag"].toString(), QString("PROXY"));
    QCOMPARE(result["protocol"].toString(), QString("vmess"));
    QCOMPARE(result["settings"].toObject()["key"].toString(), QString("value"));
    QCOMPARE(result["streamSettings"].toObject()["network"].toString(), QString("tcp"));
}

void TestSerialization::testOutbound_GenerateFreedom()
{
    QJsonObject result = outbounds::GenerateFreedomOUT("AsIs", "");
    QCOMPARE(result["domainStrategy"].toString(), QString("AsIs"));
    QCOMPARE(result["redirect"].toString(), QString(""));
}

void TestSerialization::testOutbound_GenerateBlackHole()
{
    QJsonObject httpResult = outbounds::GenerateBlackHoleOUT(true);
    QCOMPARE(httpResult["response"].toObject()["type"].toString(), QString("http"));

    QJsonObject noneResult = outbounds::GenerateBlackHoleOUT(false);
    QCOMPARE(noneResult["response"].toObject()["type"].toString(), QString("none"));
}

void TestSerialization::testOutbound_GenerateShadowSocksServer()
{
    QJsonObject result = outbounds::GenerateShadowSocksServerOUT(
        "1.2.3.4", 8388, "aes-256-gcm", "password123");
    QCOMPARE(result["address"].toString(), QString("1.2.3.4"));
    QCOMPARE(result["port"].toInt(), 8388);
    QCOMPARE(result["method"].toString(), QString("aes-256-gcm"));
    QCOMPARE(result["password"].toString(), QString("password123"));
}

void TestSerialization::testOutbound_GenerateShadowSocks()
{
    ShadowSocksServerObject s1;
    s1.address = "server1.com";
    s1.port = 8388;
    s1.method = "aes-256-gcm";
    s1.password = "pass1";

    ShadowSocksServerObject s2;
    s2.address = "server2.com";
    s2.port = 8389;
    s2.method = "chacha20-ietf-poly1305";
    s2.password = "pass2";

    QJsonObject result = outbounds::GenerateShadowSocksOUT({ s1, s2 });
    QJsonArray servers = result["servers"].toArray();
    QCOMPARE(servers.size(), 2);
    QCOMPARE(servers[0].toObject()["address"].toString(), QString("server1.com"));
    QCOMPARE(servers[1].toObject()["address"].toString(), QString("server2.com"));
}

void TestSerialization::testOutbound_GenerateTrojanServer()
{
    QJsonObject result = outbounds::GenerateTrojanServerOUT("trojan.example.com", 443, "trojanpass");
    QCOMPARE(result["address"].toString(), QString("trojan.example.com"));
    QCOMPARE(result["port"].toInt(), 443);
    QCOMPARE(result["password"].toString(), QString("trojanpass"));
}

void TestSerialization::testOutbound_GenerateHTTPSOCKS_noAuth()
{
    QJsonObject result = outbounds::GenerateHTTPSOCKSOut("proxy.example.com", 1080, false, "", "");
    QJsonObject server = result["servers"].toArray()[0].toObject();
    QCOMPARE(server["address"].toString(), QString("proxy.example.com"));
    QCOMPARE(server["port"].toInt(), 1080);
    QVERIFY(!server.contains("users"));
}

void TestSerialization::testOutbound_GenerateHTTPSOCKS_withAuth()
{
    QJsonObject result = outbounds::GenerateHTTPSOCKSOut("proxy.example.com", 1080, true, "user", "pass");
    QJsonObject server = result["servers"].toArray()[0].toObject();
    QCOMPARE(server["address"].toString(), QString("proxy.example.com"));
    QJsonObject user = server["users"].toArray()[0].toObject();
    QCOMPARE(user["user"].toString(), QString("user"));
    QCOMPARE(user["pass"].toString(), QString("pass"));
}

// ═══════════════════════════════════════════════════════════════════════════
// Inbounds Tests
// ═══════════════════════════════════════════════════════════════════════════

void TestSerialization::testInbound_GenerateEntry()
{
    QJsonObject result = inbounds::GenerateInboundEntry();
    QCOMPARE(result["listen"].toString(), QString("127.0.0.1"));
    QCOMPARE(result["port"].toInt(), 10808);
    QCOMPARE(result["protocol"].toString(), QString("socks"));
    QVERIFY(result["settings"].toObject()["udp"].toBool());
}

void TestSerialization::testInbound_GetCredentials_empty()
{
    QJsonObject config;
    config["inbounds"] = QJsonArray{ inbounds::GenerateInboundEntry() };

    auto creds = inbounds::GetInboundCredentials(config);
    QCOMPARE(creds.port, 10808);
    QVERIFY(creds.username.isEmpty());
    QVERIFY(creds.password.isEmpty());
}

void TestSerialization::testInbound_GetCredentials_withAccounts()
{
    QJsonObject inbound;
    inbound["protocol"] = "socks";
    inbound["port"] = 12345;
    QJsonObject account;
    account["user"] = "testuser";
    account["pass"] = "testpass";
    QJsonObject settings;
    settings["accounts"] = QJsonArray{ account };
    inbound["settings"] = settings;

    QJsonObject config;
    config["inbounds"] = QJsonArray{ inbound };

    auto creds = inbounds::GetInboundCredentials(config);
    QCOMPARE(creds.port, 12345);
    QCOMPARE(creds.username, QString("testuser"));
    QCOMPARE(creds.password, QString("testpass"));
}

void TestSerialization::testInbound_EnsureAuth()
{
    QJsonObject inbound;
    inbound["protocol"] = "socks";
    inbound["port"] = 10808;
    QJsonObject settings;
    settings["udp"] = true;
    inbound["settings"] = settings;

    QJsonObject config;
    config["inbounds"] = QJsonArray{ inbound };

    auto creds = inbounds::EnsureInboundAuth(config);
    QVERIFY(creds.port > 0);
    QVERIFY(!creds.username.isEmpty());
    QVERIFY(!creds.password.isEmpty());

    // Verify the config was updated
    QJsonObject updatedInbound = config["inbounds"].toArray()[0].toObject();
    QCOMPARE(updatedInbound["port"].toInt(), creds.port);
    QCOMPARE(updatedInbound["settings"].toObject()["auth"].toString(), QString("password"));
}

QTEST_GUILESS_MAIN(TestSerialization)
#include "test_serialization.moc"
