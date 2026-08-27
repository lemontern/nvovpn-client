#ifndef NVOSERVICEEXTRAS_H
#define NVOSERVICEEXTRAS_H

#include <QJsonObject>
#include <QMutex>

/**
 * §5.7: данные для Android-службы, которых нет в штатном vpnConfig Amnezia:
 *   nvo_candidates — готовые vpnConfig-JSON запасных путей (VLESS direct / VLESS CDN), по которым служба
 *                    сама переключается при обрыве (детектор живости живёт в службе — C++ в фоне спит);
 *   nvo_liveness   — ping-URL для активного проба через туннель, api_base+token для телеметрии, server_id, path.
 * NvoApiController готовит их из ответа POST /connect, AndroidController::start подмешивает в JSON службы.
 * Только память процесса: на диск не пишем (в JSON есть токен и UUID).
 */
class NvoServiceExtras
{
public:
    static void set(const QJsonObject &extras)
    {
        QMutexLocker locker(&mutex());
        object() = extras;
    }

    static void clear()
    {
        set(QJsonObject());
    }

    static QJsonObject current()
    {
        QMutexLocker locker(&mutex());
        return object();
    }

private:
    static QJsonObject &object()
    {
        static QJsonObject obj;
        return obj;
    }

    static QMutex &mutex()
    {
        static QMutex m;
        return m;
    }
};

#endif // NVOSERVICEEXTRAS_H
