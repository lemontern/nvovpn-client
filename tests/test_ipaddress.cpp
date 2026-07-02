#include <QtTest/QtTest>
#include <QHostAddress>

#include "ipaddress.h"

class TestIPAddress : public QObject
{
    Q_OBJECT

private slots:
    void testConstructor_ipv4WithPrefix();
    void testConstructor_ipv4WithoutPrefix();
    void testConstructor_ipv6WithPrefix();
    void testConstructor_ipv6WithoutPrefix();
    void testConstructor_hostAddress();
    void testConstructor_hostAddressWithPrefix();
    void testConstructor_copy();

    void testToString();

    void testType_ipv4();
    void testType_ipv6();

    void testNetmask_ipv4_prefix24();
    void testNetmask_ipv4_prefix16();
    void testNetmask_ipv4_prefix32();
    void testNetmask_ipv4_prefix0();
    void testNetmask_ipv6();

    void testHostmask_ipv4_prefix24();
    void testHostmask_ipv4_prefix32();
    void testHostmask_ipv4_prefix0();

    void testBroadcastAddress_ipv4_prefix24();
    void testBroadcastAddress_ipv4_prefix32();

    void testContains_ipv4_sameNetwork();
    void testContains_ipv4_differentNetwork();
    void testContains_ipv4_prefix0();
    void testContains_differentProtocol();

    void testOverlaps_overlapping();
    void testOverlaps_nonOverlapping();

    void testEquality();
    void testInequality();

    void testSubnetOf_true();
    void testSubnetOf_false();
    void testSubnetOf_differentProtocol();

    void testSubnets_ipv4();
    void testSubnets_ipv4_host();
    void testSubnets_ipv6();

    void testExcludeAddresses_basic();
    void testExcludeAddresses_noOverlap();
    void testExcludeAddresses_static();
};

void TestIPAddress::testConstructor_ipv4WithPrefix()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.address(), QHostAddress("192.168.1.0"));
    QCOMPARE(ip.prefixLength(), 24);
}

void TestIPAddress::testConstructor_ipv4WithoutPrefix()
{
    IPAddress ip("192.168.1.1");
    QCOMPARE(ip.address(), QHostAddress("192.168.1.1"));
    QCOMPARE(ip.prefixLength(), 32);
}

void TestIPAddress::testConstructor_ipv6WithPrefix()
{
    IPAddress ip("2001:db8::/32");
    QCOMPARE(ip.address(), QHostAddress("2001:db8::"));
    QCOMPARE(ip.prefixLength(), 32);
}

void TestIPAddress::testConstructor_ipv6WithoutPrefix()
{
    IPAddress ip("2001:db8::1");
    QCOMPARE(ip.address(), QHostAddress("2001:db8::1"));
    QCOMPARE(ip.prefixLength(), 128);
}

void TestIPAddress::testConstructor_hostAddress()
{
    QHostAddress addr("10.0.0.1");
    IPAddress ip(addr);
    QCOMPARE(ip.address(), addr);
    QCOMPARE(ip.prefixLength(), 32);
}

void TestIPAddress::testConstructor_hostAddressWithPrefix()
{
    QHostAddress addr("10.0.0.0");
    IPAddress ip(addr, 8);
    QCOMPARE(ip.address(), addr);
    QCOMPARE(ip.prefixLength(), 8);
}

void TestIPAddress::testConstructor_copy()
{
    IPAddress original("192.168.1.0/24");
    IPAddress copy(original);
    QCOMPARE(copy.address(), original.address());
    QCOMPARE(copy.prefixLength(), original.prefixLength());
}

void TestIPAddress::testToString()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.toString(), QString("192.168.1.0/24"));

    IPAddress host("10.0.0.1");
    QCOMPARE(host.toString(), QString("10.0.0.1/32"));
}

void TestIPAddress::testType_ipv4()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.type(), QAbstractSocket::IPv4Protocol);
}

void TestIPAddress::testType_ipv6()
{
    IPAddress ip("2001:db8::/32");
    QCOMPARE(ip.type(), QAbstractSocket::IPv6Protocol);
}

void TestIPAddress::testNetmask_ipv4_prefix24()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.netmask(), QHostAddress("255.255.255.0"));
}

void TestIPAddress::testNetmask_ipv4_prefix16()
{
    IPAddress ip("172.16.0.0/16");
    QCOMPARE(ip.netmask(), QHostAddress("255.255.0.0"));
}

void TestIPAddress::testNetmask_ipv4_prefix32()
{
    IPAddress ip("192.168.1.1/32");
    QCOMPARE(ip.netmask(), QHostAddress("255.255.255.255"));
}

void TestIPAddress::testNetmask_ipv4_prefix0()
{
    IPAddress ip(QHostAddress("0.0.0.0"), 0);
    QCOMPARE(ip.netmask(), QHostAddress("0.0.0.0"));
}

void TestIPAddress::testNetmask_ipv6()
{
    IPAddress ip("2001:db8::/32");
    QCOMPARE(ip.netmask(), QHostAddress("ffff:ffff::"));
}

void TestIPAddress::testHostmask_ipv4_prefix24()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.hostmask(), QHostAddress("0.0.0.255"));
}

void TestIPAddress::testHostmask_ipv4_prefix32()
{
    IPAddress ip("192.168.1.1/32");
    QCOMPARE(ip.hostmask(), QHostAddress("0.0.0.0"));
}

void TestIPAddress::testHostmask_ipv4_prefix0()
{
    IPAddress ip(QHostAddress("0.0.0.0"), 0);
    QCOMPARE(ip.hostmask(), QHostAddress("255.255.255.255"));
}

void TestIPAddress::testBroadcastAddress_ipv4_prefix24()
{
    IPAddress ip("192.168.1.0/24");
    QCOMPARE(ip.broadcastAddress(), QHostAddress("192.168.1.255"));
}

void TestIPAddress::testBroadcastAddress_ipv4_prefix32()
{
    IPAddress ip("192.168.1.1/32");
    QCOMPARE(ip.broadcastAddress(), QHostAddress("192.168.1.1"));
}

void TestIPAddress::testContains_ipv4_sameNetwork()
{
    IPAddress network("192.168.1.0/24");
    QVERIFY(network.contains(QHostAddress("192.168.1.1")));
    QVERIFY(network.contains(QHostAddress("192.168.1.100")));
    QVERIFY(network.contains(QHostAddress("192.168.1.254")));
}

void TestIPAddress::testContains_ipv4_differentNetwork()
{
    IPAddress network("192.168.1.0/24");
    QVERIFY(!network.contains(QHostAddress("192.168.2.1")));
    QVERIFY(!network.contains(QHostAddress("10.0.0.1")));
}

void TestIPAddress::testContains_ipv4_prefix0()
{
    IPAddress everything(QHostAddress("0.0.0.0"), 0);
    QVERIFY(everything.contains(QHostAddress("192.168.1.1")));
    QVERIFY(everything.contains(QHostAddress("10.0.0.1")));
    QVERIFY(everything.contains(QHostAddress("255.255.255.255")));
}

void TestIPAddress::testContains_differentProtocol()
{
    IPAddress ipv4("192.168.1.0/24");
    QVERIFY(!ipv4.contains(QHostAddress("2001:db8::1")));
}

void TestIPAddress::testOverlaps_overlapping()
{
    IPAddress a("192.168.0.0/16");
    IPAddress b("192.168.1.0/24");
    QVERIFY(a.overlaps(b));
    QVERIFY(b.overlaps(a));
}

void TestIPAddress::testOverlaps_nonOverlapping()
{
    IPAddress a("192.168.1.0/24");
    IPAddress b("192.168.2.0/24");
    QVERIFY(!a.overlaps(b));
    QVERIFY(!b.overlaps(a));
}

void TestIPAddress::testEquality()
{
    IPAddress a("192.168.1.0/24");
    IPAddress b("192.168.1.0/24");
    QVERIFY(a == b);
}

void TestIPAddress::testInequality()
{
    IPAddress a("192.168.1.0/24");
    IPAddress b("192.168.1.0/25");
    QVERIFY(a != b);

    IPAddress c("192.168.2.0/24");
    QVERIFY(a != c);
}

void TestIPAddress::testSubnetOf_true()
{
    IPAddress child("192.168.1.0/24");
    IPAddress parent("192.168.0.0/16");
    QVERIFY(child.subnetOf(parent));
}

void TestIPAddress::testSubnetOf_false()
{
    IPAddress parent("192.168.0.0/16");
    IPAddress child("192.168.1.0/24");
    QVERIFY(!parent.subnetOf(child));
}

void TestIPAddress::testSubnetOf_differentProtocol()
{
    IPAddress ipv4("192.168.1.0/24");
    IPAddress ipv6("2001:db8::/32");
    QVERIFY(!ipv4.subnetOf(ipv6));
}

void TestIPAddress::testSubnets_ipv4()
{
    IPAddress ip("192.168.0.0/23");
    QList<IPAddress> sn = ip.subnets();
    QCOMPARE(sn.size(), 2);
    QCOMPARE(sn[0].prefixLength(), 24);
    QCOMPARE(sn[1].prefixLength(), 24);
    QCOMPARE(sn[0].address(), QHostAddress("192.168.0.0"));
    QCOMPARE(sn[1].address(), QHostAddress("192.168.1.0"));
}

void TestIPAddress::testSubnets_ipv4_host()
{
    IPAddress ip("192.168.1.1/32");
    QList<IPAddress> sn = ip.subnets();
    QCOMPARE(sn.size(), 1);
    QCOMPARE(sn[0], ip);
}

void TestIPAddress::testSubnets_ipv6()
{
    IPAddress ip("2001:db8::/31");
    QList<IPAddress> sn = ip.subnets();
    QCOMPARE(sn.size(), 2);
    QCOMPARE(sn[0].prefixLength(), 32);
    QCOMPARE(sn[1].prefixLength(), 32);
}

void TestIPAddress::testExcludeAddresses_basic()
{
    IPAddress parent("192.168.0.0/23");
    IPAddress exclude("192.168.1.0/24");
    QList<IPAddress> result = parent.excludeAddresses(exclude);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0], IPAddress("192.168.0.0/24"));
}

void TestIPAddress::testExcludeAddresses_noOverlap()
{
    QList<IPAddress> source = { IPAddress("192.168.1.0/24") };
    QList<IPAddress> excludes = { IPAddress("10.0.0.0/8") };
    QList<IPAddress> result = IPAddress::excludeAddresses(source, excludes);
    QCOMPARE(result.size(), 1);
    QCOMPARE(result[0], IPAddress("192.168.1.0/24"));
}

void TestIPAddress::testExcludeAddresses_static()
{
    QList<IPAddress> source = { IPAddress("192.168.0.0/16") };
    QList<IPAddress> excludes = { IPAddress("192.168.1.0/24") };
    QList<IPAddress> result = IPAddress::excludeAddresses(source, excludes);

    // Excluding /24 from /16 yields multiple subnets
    QVERIFY(result.size() > 1);

    // The excluded subnet should not be in the result
    for (const auto &ip : result) {
        QVERIFY(!ip.overlaps(IPAddress("192.168.1.0/24")));
    }
}

QTEST_GUILESS_MAIN(TestIPAddress)
#include "test_ipaddress.moc"
