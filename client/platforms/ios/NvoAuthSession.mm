#include "NvoAuthSession.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <UIKit/UIKit.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <memory>

// Окно, над которым система покажет лист входа: ключевое окно активной сцены.
@interface NvoAuthPresenter : NSObject <ASWebAuthenticationPresentationContextProviding>
@end

@implementation NvoAuthPresenter
- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession *)session
{
    (void)session;
    UIWindow *fallback = nil;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) {
            continue;
        }
        for (UIWindow *window in ((UIWindowScene *)scene).windows) {
            if (window.isKeyWindow) {
                return window;
            }
            if (!fallback) {
                fallback = window;
            }
        }
    }
    return fallback ? fallback : [[UIWindow alloc] init];
}
@end

// Сборка может идти и с ARC, и без: без ARC объект отпускаем через autorelease — безопасно даже изнутри
// обработчика самой сессии (освобождение после текущего цикла событий).
#if __has_feature(objc_arc)
#define NVO_DROP(x) ((x) = nil)
#else
#define NVO_DROP(x) do { [(x) autorelease]; (x) = nil; } while (0)
#endif

namespace {
ASWebAuthenticationSession *g_session = nil;
NvoAuthPresenter *g_presenter = nil;
// Номер текущего окна: ответ системы от закрытого нами (или старого) окна не должен трогать новое.
quint64 g_generation = 0;
}

namespace NvoAuthSession {

void open(const QString &url, std::function<void()> onUserClosed)
{
    close();
    NSURL *nsUrl = [NSURL URLWithString:url.toNSString()];
    if (!nsUrl) {
        return;
    }
    const quint64 generation = ++g_generation;
    auto callback = std::make_shared<std::function<void()>>(std::move(onUserClosed));

    // Схема обратного вызова нужна API, но вход завершается опросом /auth/poll: окно закрываем сами (close()),
    // как только опрос получил токен. Сюда система приходит, если окно закрыл человек.
    g_session = [[ASWebAuthenticationSession alloc] initWithURL:nsUrl
                                              callbackURLScheme:@"nvovpn"
                                              completionHandler:^(NSURL *callbackUrl, NSError *error) {
        (void)callbackUrl;
        if (generation != g_generation) {
            return; // окно уже закрыто приложением или заменено новым
        }
        NVO_DROP(g_session);
        NVO_DROP(g_presenter);
        const bool userCancelled = error && error.code == ASWebAuthenticationSessionErrorCodeCanceledLogin;
        if (userCancelled && *callback) {
            QMetaObject::invokeMethod(qApp, [callback]() { (*callback)(); }, Qt::QueuedConnection);
        }
    }];
    g_presenter = [[NvoAuthPresenter alloc] init];
    g_session.presentationContextProvider = g_presenter;
    // Общие cookie с Safari: если человек уже вошёл в Google в Safari, вход в одно касание.
    g_session.prefersEphemeralWebBrowserSession = NO;
    if (![g_session start]) {
        NVO_DROP(g_session);
        NVO_DROP(g_presenter);
    }
}

void close()
{
    ++g_generation; // ответ системы от этого окна больше ничего не сделает
    if (g_session) {
        [g_session cancel];
    }
    NVO_DROP(g_session);
    NVO_DROP(g_presenter);
}

} // namespace NvoAuthSession
