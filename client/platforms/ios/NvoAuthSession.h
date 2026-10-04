#ifndef NVOAUTHSESSION_H
#define NVOAUTHSESSION_H

#include <QString>
#include <functional>

// Вход через Apple / Google на iOS — во встроенном окне системы (ASWebAuthenticationSession), а не во внешнем Safari.
// App Review 03.10.2026, Guideline 4 (Design): «the user is taken to the default web browser to sign in or register»
// — 1.0.5 (263) отклонена. Страница та же (/app/login/<провайдер>?ds=…), вход по-прежнему завершает опрос /auth/poll.
namespace NvoAuthSession {

// Открыть страницу входа. onUserClosed — человек сам закрыл окно (кнопка «Отменить»), без завершения входа.
void open(const QString &url, std::function<void()> onUserClosed);

// Закрыть окно (вход завершён, отменён или истёк). Без открытого окна ничего не делает.
void close();

} // namespace NvoAuthSession

#endif // NVOAUTHSESSION_H
