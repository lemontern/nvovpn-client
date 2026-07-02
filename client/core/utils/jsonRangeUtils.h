#ifndef JSONRANGEUTILS_H
#define JSONRANGEUTILS_H

#include <QJsonObject>
#include <QString>

namespace amnezia
{

// Shared utilities for reading and writing integer range objects ({from, to})
// in JSON. Used by xray configurator and installer code that repeatedly
// serializes/deserializes range pairs.
namespace JsonRangeUtils
{

// Writes a {from, to} range to parent[key]. Skips if both minV and maxV are empty.
// Falls back to fallbackMin/fallbackMax when one value is empty.
void writeIntRange(QJsonObject &parent, const char *key,
                   const QString &minV, const QString &maxV,
                   const char *fallbackMin = "0", const char *fallbackMax = "0");

// Reads a {from, to} range from parent[key] into minOut and maxOut.
void readIntRange(const QJsonObject &parent, const char *key,
                  QString &minOut, QString &maxOut);

} // namespace JsonRangeUtils

} // namespace amnezia

#endif // JSONRANGEUTILS_H
