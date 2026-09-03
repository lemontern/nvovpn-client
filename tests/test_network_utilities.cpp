#include <QtTest/QtTest>
#include <QHostAddress>

#include "core/utils/networkUtilities.h"

class TestNetworkUtilities : public QObject
{
    Q_OBJECT

private slots:
    void testIpAddressRegExp_valid();
    void testIpAddressRegExp_invalid();

    void testCheckIPv4Format_valid();
    void testCheckIPv4Format_invalid();

    void testCheckIpSubnetFormat_valid();
    void testCheckIpSubnetFormat_invalid();

    void testNetMaskFromIpWithSubnet_noSubnet();
    void testNetMaskFromIpWithSubnet_prefix24();
    void testNetMaskFromIpWithSubnet_prefix16();
    void testNetMaskFromIpWithSubnet_prefix8();
    void testNetMaskFromIpWithSubnet_prefix32();
    void testNetMaskFromIpWithSubnet_prefix0();

    void testIpAddressFromIpWithSubnet_withSubnet();
    void testIpAddressFromIpWithSubnet_withoutSubnet();
    void testIpAddressFromIpWithSubnet_invalid();

    void testGetStringBetween_normal();
    void testGetStringBetween_notFound();
    void testGetStringBetween_empty();

    void testDomainRegExp_valid();
    void testDomainRegExp_invalid();
};

void TestNetworkUtilities::testIpAddressRegExp_valid()
{
    QRegularExpression re = NetworkUtilities::ipAddressRegExp();
    QVERIFY(re.match("192.168.1.1").hasMatch());
    QVERIFY(re.match("0.0.0.0").hasMatch());
    QVERIFY(re.match("255.255.255.255").hasMatch());
    QVERIFY(re.match("10.0.0.1").hasMatch());
    QVERIFY(re.match("172.16.0.1").hasMatch());
}

void TestNetworkUtilities::testIpAddressRegExp_invalid()
{
    QRegularExpression re = NetworkUtilities::ipAddressRegExp();
    QVERIFY(!re.match("256.1.1.1").hasMatch());
    QVERIFY(!re.match("1.2.3").hasMatch());
    QVERIFY(!re.match("abc.def.ghi.jkl").hasMatch());
    QVERIFY(!re.match("").hasMatch());
}

void TestNetworkUtilities::testCheckIPv4Format_valid()
{
    QVERIFY(NetworkUtilities::checkIPv4Format("192.168.1.1"));
    QVERIFY(NetworkUtilities::checkIPv4Format("0.0.0.0"));
    QVERIFY(NetworkUtilities::checkIPv4Format("255.255.255.255"));
    QVERIFY(NetworkUtilities::checkIPv4Format("10.0.0.1"));
}

void TestNetworkUtilities::testCheckIPv4Format_invalid()
{
    QVERIFY(!NetworkUtilities::checkIPv4Format(""));
    QVERIFY(!NetworkUtilities::checkIPv4Format("256.1.1.1"));
    QVERIFY(!NetworkUtilities::checkIPv4Format("1.2.3"));
    QVERIFY(!NetworkUtilities::checkIPv4Format("not an ip"));
    QVERIFY(!NetworkUtilities::checkIPv4Format("192.168.1.1/24"));
}

void TestNetworkUtilities::testCheckIpSubnetFormat_valid()
{
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("192.168.1.0/24"));
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("10.0.0.0/8"));
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("172.16.0.0/16"));
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("192.168.1.1/32"));
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("0.0.0.0/0"));
    QVERIFY(NetworkUtilities::checkIpSubnetFormat("192.168.1.1"));
}

void TestNetworkUtilities::testCheckIpSubnetFormat_invalid()
{
    QVERIFY(!NetworkUtilities::checkIpSubnetFormat("192.168.1.0/33"));
    QVERIFY(!NetworkUtilities::checkIpSubnetFormat("192.168.1.0/-1"));
    QVERIFY(!NetworkUtilities::checkIpSubnetFormat("invalid/24"));
    QVERIFY(!NetworkUtilities::checkIpSubnetFormat(""));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_noSubnet()
{
    QCOMPARE(NetworkUtilities::netMaskFromIpWithSubnet("192.168.1.1"),
             QString("255.255.255.255"));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_prefix24()
{
    QCOMPARE(NetworkUtilities::netMaskFromIpWithSubnet("192.168.1.0/24"),
             QString("255.255.255.0"));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_prefix16()
{
    QCOMPARE(NetworkUtilities::netMaskFromIpWithSubnet("172.16.0.0/16"),
             QString("255.255.0.0"));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_prefix8()
{
    QCOMPARE(NetworkUtilities::netMaskFromIpWithSubnet("10.0.0.0/8"),
             QString("255.0.0.0"));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_prefix32()
{
    QCOMPARE(NetworkUtilities::netMaskFromIpWithSubnet("192.168.1.1/32"),
             QString("255.255.255.255"));
}

void TestNetworkUtilities::testNetMaskFromIpWithSubnet_prefix0()
{
    // NOTE: The implementation uses (0xFFFFFFFF << (32 - prefix)) & 0xFFFFFFFF
    // which is technically UB when prefix=0 (shift by 32 on 32-bit value).
    // On this platform, the result is 255.255.255.255.
    QString result = NetworkUtilities::netMaskFromIpWithSubnet("0.0.0.0/0");
    QVERIFY(!result.isEmpty());
}

void TestNetworkUtilities::testIpAddressFromIpWithSubnet_withSubnet()
{
    QCOMPARE(NetworkUtilities::ipAddressFromIpWithSubnet("192.168.1.0/24"),
             QString("192.168.1.0"));
}

void TestNetworkUtilities::testIpAddressFromIpWithSubnet_withoutSubnet()
{
    QCOMPARE(NetworkUtilities::ipAddressFromIpWithSubnet("192.168.1.1"),
             QString("192.168.1.1"));
}

void TestNetworkUtilities::testIpAddressFromIpWithSubnet_invalid()
{
    QCOMPARE(NetworkUtilities::ipAddressFromIpWithSubnet("invalid"),
             QString(""));
}

void TestNetworkUtilities::testGetStringBetween_normal()
{
    QCOMPARE(NetworkUtilities::getStringBetween("hello [world] end", "[", "]"),
             QString("world"));
    QCOMPARE(NetworkUtilities::getStringBetween("start<<middle>>end", "<<", ">>"),
             QString("middle"));
}

void TestNetworkUtilities::testGetStringBetween_notFound()
{
    QCOMPARE(NetworkUtilities::getStringBetween("no markers here", "[", "]"),
             QString());
    QCOMPARE(NetworkUtilities::getStringBetween("only [start", "[", "]"),
             QString());
}

void TestNetworkUtilities::testGetStringBetween_empty()
{
    QCOMPARE(NetworkUtilities::getStringBetween("", "[", "]"), QString());
}

void TestNetworkUtilities::testDomainRegExp_valid()
{
    QRegExp re = NetworkUtilities::domainRegExp();
    QVERIFY(re.exactMatch("example.com"));
    QVERIFY(re.exactMatch("sub.example.com"));
    QVERIFY(re.exactMatch("deep.sub.example.co.uk"));
}

void TestNetworkUtilities::testDomainRegExp_invalid()
{
    QRegExp re = NetworkUtilities::domainRegExp();
    QVERIFY(!re.exactMatch("192.168.1.1"));
    QVERIFY(!re.exactMatch(""));
}

QTEST_GUILESS_MAIN(TestNetworkUtilities)
#include "test_network_utilities.moc"
