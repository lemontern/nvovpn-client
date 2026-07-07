#include "jsonRangeUtils.h"

namespace amnezia::JsonRangeUtils
{

void writeIntRange(QJsonObject &parent, const char *key,
                   const QString &minV, const QString &maxV,
                   const char *fallbackMin, const char *fallbackMax)
{
    if (minV.isEmpty() && maxV.isEmpty())
        return;
    QJsonObject r;
    r[QStringLiteral("from")] = minV.isEmpty() ? QString::fromLatin1(fallbackMin).toInt() : minV.toInt();
    r[QStringLiteral("to")] = maxV.isEmpty() ? QString::fromLatin1(fallbackMax).toInt() : maxV.toInt();
    parent[QString::fromUtf8(key)] = r;
}

void readIntRange(const QJsonObject &parent, const char *key,
                  QString &minOut, QString &maxOut)
{
    QJsonObject r = parent.value(QLatin1String(key)).toObject();
    if (!r.isEmpty()) {
        minOut = QString::number(r.value("from").toInt());
        maxOut = QString::number(r.value("to").toInt());
    }
}

} // namespace amnezia::JsonRangeUtils
