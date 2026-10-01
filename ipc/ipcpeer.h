#ifndef IPCPEER_H
#define IPCPEER_H

// 01.10.2026 (аудит D-12). Служба NvoVPN на Windows работает от SYSTEM и слушает именованный канал с
// WorldAccessOption — иначе к нему не подключится приложение обычного пользователя. Но тогда командовать
// службой (маршруты, запуск tun2socks с чужим socks5, встроенный xray с произвольным конфигом, снятие
// KillSwitch) мог ЛЮБОЙ локальный процесс. Здесь — проверка, что на другом конце канала именно наш
// NvoVPN.exe из каталога службы (установщик кладёт их рядом). На macOS/Linux служба не выпускается —
// там проверка пропускается.

#include <QLocalSocket>
#include <QString>

#ifdef Q_OS_WIN
    #include <QCoreApplication>
    #include <QDir>
    #include <QDebug>
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace amnezia {

inline bool ipcPeerIsTrusted(QLocalSocket *socket)
{
#ifdef Q_OS_WIN
    if (!socket) {
        return false;
    }
    HANDLE pipe = reinterpret_cast<HANDLE>(socket->socketDescriptor());
    ULONG pid = 0;
    if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE || !GetNamedPipeClientProcessId(pipe, &pid)) {
        qWarning() << "IPC: cannot identify peer process, error" << GetLastError();
        return false;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        qWarning() << "IPC: cannot open peer process" << pid << "error" << GetLastError();
        return false;
    }
    wchar_t buffer[MAX_PATH * 4];
    DWORD length = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const bool queried = QueryFullProcessImageNameW(process, 0, buffer, &length) != 0;
    CloseHandle(process);
    if (!queried) {
        qWarning() << "IPC: cannot read peer image path, pid" << pid << "error" << GetLastError();
        return false;
    }
    const QString peer = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromWCharArray(buffer, static_cast<int>(length))));
    const QString expected = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/NvoVPN.exe"));
    if (peer.compare(expected, Qt::CaseInsensitive) != 0) {
        qWarning() << "IPC: connection from untrusted process rejected:" << peer << "pid" << pid << "(expected" << expected << ")";
        return false;
    }
    return true;
#else
    Q_UNUSED(socket)
    return true;
#endif
}

} // namespace amnezia

#endif // IPCPEER_H
