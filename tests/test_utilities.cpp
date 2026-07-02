#include <QtTest/QtTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "core/utils/utilities.h"

class TestUtilities : public QObject
{
    Q_OBJECT

private slots:
    void testGetRandomString_length();
    void testGetRandomString_characters();
    void testGetRandomString_uniqueness();
    void testGetRandomString_zeroLength();

    void testVerifyJsonString_valid();
    void testVerifyJsonString_invalid();
    void testVerifyJsonString_empty();

    void testJsonFromString_validObject();
    void testJsonFromString_emptyString();
    void testJsonFromString_invalidJson();

    void testJsonToString_object();
    void testJsonToString_array();
    void testJsonToString_compact();

    void testSafeBase64Decode_standard();
    void testSafeBase64Decode_urlSafe();
    void testSafeBase64Decode_noPadding();

    void testInitializePath_creates();
    void testInitializePath_existing();

    void testCreateEmptyFile();

    void testExecutable_relative();
};

void TestUtilities::testGetRandomString_length()
{
    QCOMPARE(Utils::getRandomString(0).length(), 0);
    QCOMPARE(Utils::getRandomString(1).length(), 1);
    QCOMPARE(Utils::getRandomString(10).length(), 10);
    QCOMPARE(Utils::getRandomString(100).length(), 100);
}

void TestUtilities::testGetRandomString_characters()
{
    const QString possibleChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString result = Utils::getRandomString(1000);
    for (const QChar &c : result) {
        QVERIFY2(possibleChars.contains(c),
                 qPrintable(QString("Unexpected character: '%1'").arg(c)));
    }
}

void TestUtilities::testGetRandomString_uniqueness()
{
    QString s1 = Utils::getRandomString(32);
    QString s2 = Utils::getRandomString(32);
    QVERIFY(s1 != s2);
}

void TestUtilities::testGetRandomString_zeroLength()
{
    QCOMPARE(Utils::getRandomString(0), QString());
}

void TestUtilities::testVerifyJsonString_valid()
{
    QCOMPARE(Utils::VerifyJsonString(R"({"key": "value"})"), QString(""));
    QCOMPARE(Utils::VerifyJsonString(R"([1, 2, 3])"), QString(""));
    QCOMPARE(Utils::VerifyJsonString(R"({"nested": {"a": 1}})"), QString(""));
}

void TestUtilities::testVerifyJsonString_invalid()
{
    QVERIFY(!Utils::VerifyJsonString("{invalid}").isEmpty());
    QVERIFY(!Utils::VerifyJsonString("not json at all").isEmpty());
    QVERIFY(!Utils::VerifyJsonString("{\"key\": }").isEmpty());
}

void TestUtilities::testVerifyJsonString_empty()
{
    QVERIFY(!Utils::VerifyJsonString("").isEmpty());
}

void TestUtilities::testJsonFromString_validObject()
{
    QJsonObject obj = Utils::JsonFromString(R"({"name": "test", "value": 42})");
    QCOMPARE(obj["name"].toString(), QString("test"));
    QCOMPARE(obj["value"].toInt(), 42);
}

void TestUtilities::testJsonFromString_emptyString()
{
    QJsonObject obj = Utils::JsonFromString("");
    QVERIFY(obj.isEmpty());
}

void TestUtilities::testJsonFromString_invalidJson()
{
    QJsonObject obj = Utils::JsonFromString("not json");
    QVERIFY(obj.isEmpty());
}

void TestUtilities::testJsonToString_object()
{
    QJsonObject obj;
    obj["key"] = "value";
    QString result = Utils::JsonToString(obj, QJsonDocument::Indented);
    QVERIFY(result.contains("\"key\""));
    QVERIFY(result.contains("\"value\""));
}

void TestUtilities::testJsonToString_array()
{
    QJsonArray arr;
    arr.append(1);
    arr.append(2);
    arr.append(3);
    QString result = Utils::JsonToString(arr, QJsonDocument::Compact);
    QCOMPARE(result.trimmed(), QString("[1,2,3]"));
}

void TestUtilities::testJsonToString_compact()
{
    QJsonObject obj;
    obj["a"] = 1;
    QString result = Utils::JsonToString(obj, QJsonDocument::Compact);
    QVERIFY(!result.contains('\n'));
}

void TestUtilities::testSafeBase64Decode_standard()
{
    // "hello" in base64 is "aGVsbG8="
    QString decoded = Utils::SafeBase64Decode("aGVsbG8=");
    QCOMPARE(decoded, QString("hello"));
}

void TestUtilities::testSafeBase64Decode_urlSafe()
{
    // URL-safe base64 uses - instead of + and _ instead of /
    // Test with a string that would contain + and / in standard base64
    QString standard = "aGVsbG8+d29ybGQ/";  // contains + and /
    QString urlSafe = "aGVsbG8-d29ybGQ_";   // URL-safe equivalent
    QString decoded = Utils::SafeBase64Decode(urlSafe);
    QString decodedStd = Utils::SafeBase64Decode(standard);
    QCOMPARE(decoded, decodedStd);
}

void TestUtilities::testSafeBase64Decode_noPadding()
{
    // base64 without padding (OmitTrailingEquals)
    QString decoded = Utils::SafeBase64Decode("aGVsbG8");
    QCOMPARE(decoded, QString("hello"));
}

void TestUtilities::testInitializePath_creates()
{
    QString testPath = QDir::tempPath() + "/nvovpn_test_" + Utils::getRandomString(8);
    QVERIFY(Utils::initializePath(testPath));
    QVERIFY(QDir(testPath).exists());
    QDir(testPath).removeRecursively();
}

void TestUtilities::testInitializePath_existing()
{
    QString testPath = QDir::tempPath();
    QVERIFY(Utils::initializePath(testPath));
}

void TestUtilities::testCreateEmptyFile()
{
    QString testFile = QDir::tempPath() + "/nvovpn_test_file_" + Utils::getRandomString(8);
    QVERIFY(Utils::createEmptyFile(testFile));
    QVERIFY(QFile::exists(testFile));
    QFileInfo fi(testFile);
    QCOMPARE(fi.size(), qint64(0));
    QFile::remove(testFile);
}

void TestUtilities::testExecutable_relative()
{
    QString result = Utils::executable("myapp", false);
    QCOMPARE(result, QString("myapp"));
}

QTEST_GUILESS_MAIN(TestUtilities)
#include "test_utilities.moc"
