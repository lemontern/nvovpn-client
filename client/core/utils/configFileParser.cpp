#include "configFileParser.h"

namespace amnezia::ConfigFileParser
{

QMap<QString, QString> parseIniStyle(const QString &configText)
{
    QMap<QString, QString> result;
    const auto lines = configText.split("\n");
    for (const auto &line : lines) {
        auto trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("[") && trimmedLine.endsWith("]")) {
            continue;
        }
        QStringList parts = trimmedLine.split(" = ");
        if (parts.count() == 2) {
            result.insert(parts[0].trimmed(), parts[1].trimmed());
        }
    }
    return result;
}

QMap<QString, QString> parseSpaceSeparated(const QString &configText)
{
    QMap<QString, QString> result;
    const auto lines = configText.split("\n");
    for (const auto &line : lines) {
        auto trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("#") || trimmedLine.isEmpty()) {
            continue;
        }
        QStringList parts = trimmedLine.split(" ");
        if (parts.count() >= 2) {
            QString key = parts[0];
            QString value = parts.mid(1).join(" ");
            result.insert(key, value);
        }
    }
    return result;
}

} // namespace amnezia::ConfigFileParser
