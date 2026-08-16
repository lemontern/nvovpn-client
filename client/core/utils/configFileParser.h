#ifndef CONFIGFILEPARSER_H
#define CONFIGFILEPARSER_H

#include <QMap>
#include <QString>
#include <QStringList>

namespace amnezia
{

// Shared utilities for parsing simple key-value configuration files.
// Used by WireGuard, AWG, and OpenVPN installers that all read server
// configs with slightly different syntax.
namespace ConfigFileParser
{

// Parses INI-style configs (e.g. WireGuard/AWG) that use " = " as delimiter
// and skip section headers like "[Interface]".
QMap<QString, QString> parseIniStyle(const QString &configText);

// Parses space-separated configs (e.g. OpenVPN) that use the first space as
// delimiter and skip lines starting with "#".
QMap<QString, QString> parseSpaceSeparated(const QString &configText);

} // namespace ConfigFileParser

} // namespace amnezia

#endif // CONFIGFILEPARSER_H
