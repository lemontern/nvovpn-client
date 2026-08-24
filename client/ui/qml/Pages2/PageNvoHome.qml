import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

// Главный экран NvoVPN (ТЗ §12.3): одна огромная кнопка.
// Серый = выключено, спиннер = подключаем, зелёный = ЗАЩИТА ВКЛЮЧЕНА.
PageType {
    id: root

    readonly property bool connected: ConnectionController.isConnected
    readonly property bool busy: NvoApi.isBusy || ConnectionController.isConnectionInProgress

    // §12.7: авто-подключение при запуске (один раз за сессию экрана), если включено в настройках.
    property bool autoConnectTried: false

    // Таймер активной сессии: считаем время с момента подключения, сбрасываем при отключении.
    property int sessionSeconds: 0
    onConnectedChanged: {
        sessionSeconds = 0
        // In-App Review (Android): считаем успешные подключения; на 3-м NvoApi покажет
        // официальное окно оценки Google. На iOS/desktop вызов — no-op.
        if (connected) {
            NvoApi.registerSuccessfulConnection()
            // Если подключились через VLESS (фолбек по блокировке или режим «Всегда») — ненавязчиво сообщаем.
            if (NvoApi.lastConnectViaStealth)
                PageController.showNotificationMessage(qsTr("Подключено в режиме маскировки"))
        }
    }

    function formatDuration(s) {
        function p(n) { return (n < 10 ? "0" : "") + n }
        var h = Math.floor(s / 3600)
        var m = Math.floor((s % 3600) / 60)
        var sec = s % 60
        return (h > 0 ? p(h) + ":" : "") + p(m) + ":" + p(sec)
    }

    Timer {
        interval: 1000
        repeat: true
        running: root.connected
        onTriggered: root.sessionSeconds++
    }

    function currentCountryText() {
        // Google Play Metadata policy: слова «лучший/best» в UI трактуются как
        // рейтинговое утверждение (отказ 2026-07-07) — формулировки нейтральные.
        if (NvoApi.selectedServerId < 0)
            return qsTr("Авто (подбор сервера)")
        var idx = NvoServersModel.indexOfServerId(NvoApi.selectedServerId)
        var name = NvoServersModel.nameAt(idx)
        return name === "" ? qsTr("Авто (подбор сервера)") : name
    }

    function currentCountryCode() {
        if (NvoApi.selectedServerId < 0)
            return ""
        var idx = NvoServersModel.indexOfServerId(NvoApi.selectedServerId)
        return NvoServersModel.countryCodeAt(idx).toUpperCase()
    }

    Component.onCompleted: {
        if (NvoApi.isAuthenticated) {
            NvoApi.refreshServers()
            NvoApi.refreshUser()
        }
    }

    Connections {
        target: NvoApi

        function onSubscriptionRequired(message, reason) {
            // Бэкенд сам выдаёт авто-триал; сюда попадаем только при реальном отказе /connect.
            if (message && message.length > 0)
                PageController.showNotificationMessage(message)
            // email не подтверждён — остаёмся на главной с подсказкой (на сайт идти не нужно).
            if (reason === "email_unverified")
                return
            // нет тарифа/триал использован/др. — экран подписки (там продлить/пополнить).
            PageController.goToPage(PageEnum.PageNvoSubscription)
        }

        function onErrorOccurred(message) {
            PageController.showNotificationMessage(message)
        }

        // Серверы загрузились → если включён авто-коннект и есть подписка, подключаемся сами (§12.7).
        function onServersUpdated() {
            if (!root.autoConnectTried
                    && SettingsController.isAutoConnectEnabled()
                    && NvoApi.hasSubscription
                    && !ConnectionController.isConnected
                    && !ConnectionController.isConnectionInProgress) {
                root.autoConnectTried = true
                NvoApi.connectToSelectedAuto()   // помечаем как авто-старт: его ошибку не показываем диалогом
            }
        }
    }

    // ---- Верхняя строка: аккаунт ----
    RowLayout {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 16 + PageController.safeAreaTopMargin
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        Text {
            text: "NvoVPN"
            color: NvoStyle.color.paleGray
            font.family: "PT Root UI VF"
            font.weight: 800
            font.pixelSize: 20
        }

        Item { Layout.fillWidth: true }

        ImageButtonType {
            image: "qrc:/images/controls/settings.svg"
            imageColor: NvoStyle.color.paleGray
            implicitWidth: 40
            implicitHeight: 40
            onClicked: PageController.goToPageSettings()
        }
    }

    // ---- Центр: огромная кнопка ----
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: parent.width

        // ---- Центр: анимированный орб (Canvas) — «дышит» в покое, сканирует при обходе, светится при защите ----
        Item {
            id: orbHost
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 280
            implicitHeight: 280

            // Цвет орба по состоянию: зелёный = защита (красный при наведении = «отключить»),
            // фиолетовый = обход блокировки (VLESS), синий = обычное подключение (awg), фиолетовый приглушённый = покой.
            readonly property color orbColor: root.connected
                    ? (knobMouse.containsMouse ? NvoStyle.color.dangerRed : NvoStyle.color.connectedGreen)
                    : (root.busy ? (NvoApi.lastConnectViaStealth ? NvoStyle.color.nvoViolet : NvoStyle.color.nvoBlue)
                                 : NvoStyle.color.nvoViolet)
            property real t: 0

            // Драйвер анимации: чаще при коннекте, экономно в покое (батарея); пауза, когда экран не виден.
            Timer {
                interval: root.busy ? 16 : (root.connected ? 33 : 70)
                running: root.visible
                repeat: true
                onTriggered: { orbHost.t += root.busy ? 0.07 : 0.02; orb.requestPaint() }
            }

            Canvas {
                id: orb
                anchors.fill: parent
                antialiasing: true
                onPaint: {
                    var ctx = getContext("2d")
                    var w = width, h = height, cx = w / 2, cy = h / 2, R = 94
                    var c = orbHost.orbColor
                    var busy = root.busy, conn = root.connected, t = orbHost.t
                    ctx.reset()
                    ctx.clearRect(0, 0, w, h)
                    var breathe = 1 + Math.sin(t * (busy ? 2.2 : 1)) * (busy ? 0.03 : 0.015)
                    // внешнее свечение
                    var g = ctx.createRadialGradient(cx, cy, R * 0.2, cx, cy, R * 1.45 * breathe)
                    g.addColorStop(0, Qt.rgba(c.r, c.g, c.b, conn ? 0.30 : 0.20))
                    g.addColorStop(1, Qt.rgba(c.r, c.g, c.b, 0))
                    ctx.fillStyle = g
                    ctx.beginPath(); ctx.arc(cx, cy, R * 1.45 * breathe, 0, Math.PI * 2); ctx.fill()
                    // концентрические кольца
                    for (var i = 0; i < 3; i++) {
                        ctx.beginPath(); ctx.arc(cx, cy, R * (0.72 + i * 0.15) * breathe, 0, Math.PI * 2)
                        ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, 0.12 - i * 0.03); ctx.lineWidth = 1.5; ctx.stroke()
                    }
                    // основное кольцо
                    ctx.beginPath(); ctx.arc(cx, cy, R * breathe, 0, Math.PI * 2)
                    ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, 0.6); ctx.lineWidth = 3; ctx.stroke()
                    // внутренний диск
                    var gd = ctx.createRadialGradient(cx, cy - R * 0.3, 10, cx, cy, R * 0.92)
                    gd.addColorStop(0, conn ? "#163a2a" : "#221d3a")
                    gd.addColorStop(1, "#0e0c17")
                    ctx.beginPath(); ctx.arc(cx, cy, R * 0.92 * breathe, 0, Math.PI * 2); ctx.fillStyle = gd; ctx.fill()
                    // скан-дуга + орбитальная точка + центральный спиннер при подключении
                    if (busy) {
                        ctx.beginPath(); ctx.arc(cx, cy, R * breathe, t * 2.2, t * 2.2 + 1.1)
                        ctx.strokeStyle = c; ctx.lineWidth = 4; ctx.lineCap = "round"; ctx.stroke()
                        var a = t * 1.6
                        ctx.beginPath(); ctx.arc(cx + Math.cos(a) * R * 1.12, cy + Math.sin(a) * R * 1.12, 5, 0, Math.PI * 2)
                        ctx.fillStyle = c; ctx.fill()
                        ctx.beginPath(); ctx.arc(cx, cy, 22, t * 3, t * 3 + 4.2)
                        ctx.strokeStyle = c; ctx.lineWidth = 4; ctx.lineCap = "round"; ctx.stroke()
                    }
                    // пульс при защите
                    if (conn) {
                        var p = (t / (Math.PI * 2)) % 1
                        ctx.beginPath(); ctx.arc(cx, cy, R * (1 + p * 0.5), 0, Math.PI * 2)
                        ctx.strokeStyle = Qt.rgba(c.r, c.g, c.b, 0.4 * (1 - p)); ctx.lineWidth = 3; ctx.stroke()
                    }
                }
            }

            // Центр: бренд-щит (покой/защита); при наведении на «защиту» показываем ✕ (отключить).
            Image {
                anchors.centerIn: parent
                width: 104; height: 104
                source: "qrc:/images/nvoAppIconRound.png"
                sourceSize.width: 104; sourceSize.height: 104
                fillMode: Image.PreserveAspectFit
                visible: !root.busy && !(root.connected && knobMouse.containsMouse)
            }
            Text {
                anchors.centerIn: parent
                text: "✕"
                font.pixelSize: 60
                color: "white"
                visible: root.connected && knobMouse.containsMouse && !root.busy
            }

            scale: knobMouse.pressed ? 0.96 : 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

            MouseArea {
                id: knobMouse
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                // Когда подключено — кнопка активна ВСЕГДА (можно отключить), даже если состояние
                // на миг мигнуло в busy (у VLESS-CDN xray пересоздаёт WS-соединения). Иначе тап
                // «Отключить» мог не сработать.
                enabled: root.connected || !root.busy
                hoverEnabled: true
                onClicked: {
                    if (root.connected) {
                        // Именно ByUser: осознанное отключение не должно уводить на VLESS, в отличие от обрыва.
                        ConnectionController.closeConnectionByUser()
                    } else {
                        NvoApi.connectToSelected()
                    }
                }
            }
        }

        // Статус под кнопкой (вынесен из круга — лого теперь занимает весь круг).
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            horizontalAlignment: Text.AlignHCenter
            text: root.connected ? (knobMouse.containsMouse ? qsTr("Нажмите, чтобы отключить")
                                                            : qsTr("ЗАЩИТА ВКЛЮЧЕНА"))
                                 : (root.busy ? (NvoApi.lastConnectViaStealth ? qsTr("Обхожу блокировку…") : qsTr("Подключаем…"))
                                              // iOS (3.1.3(f)): без намёка на платную подписку/триал.
                                              : ((NvoApi.hasSubscription || Qt.platform.os === "ios") ? qsTr("Нажмите, чтобы включить")
                                                                        : qsTr("Подключиться бесплатно (2 дня)")))
            color: root.connected ? NvoStyle.color.connectedGreen : NvoStyle.color.paleGray
            font.family: "PT Root UI VF"
            font.weight: 800
            font.pixelSize: 20
        }

        // Пока идёт обход блокировки — спокойно поясняем, что происходит (это на пару секунд дольше awg).
        Text {
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            visible: root.busy && NvoApi.lastConnectViaStealth
            text: qsTr("Подключаемся через защищённый канал")
            color: NvoStyle.color.mutedGray
            font.pixelSize: 13
        }

        // Бейдж «Защищённый канал»: постоянно виден, когда подняли VLESS (обход блокировки или режим «Всегда»).
        // Даёт уверенность, что защищены именно там, где обычный VPN не смог пробиться.
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 2
            visible: root.connected && NvoApi.lastConnectViaStealth
            implicitWidth: secureRow.implicitWidth + 28
            implicitHeight: 32
            radius: 16
            color: NvoStyle.color.onyxBlack
            border.width: 1
            border.color: NvoStyle.color.connectedGreenDeep

            RowLayout {
                id: secureRow
                anchors.centerIn: parent
                spacing: 6
                Text { text: "🔒"; font.pixelSize: 13 }
                Text {
                    text: qsTr("Защищённый канал")
                    color: NvoStyle.color.connectedGreen
                    font.pixelSize: 13
                    font.weight: 600
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Ваш интернет защищён, IP скрыт")
            color: NvoStyle.color.mutedGray
            font.pixelSize: 13
            visible: root.connected
        }

        // Длительность активной сессии (live-таймер).
        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: root.connected
            text: qsTr("На связи: %1").arg(root.formatDuration(root.sessionSeconds))
            color: NvoStyle.color.connectedGreen
            font.family: "PT Root UI VF"
            font.weight: 700
            font.pixelSize: 15
        }

        // ---- Выбор страны ----
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            implicitWidth: countryRow.implicitWidth + 40
            implicitHeight: 56
            radius: 28
            color: NvoStyle.color.onyxBlack
            border.width: 1
            border.color: NvoStyle.color.slateGray

            RowLayout {
                id: countryRow
                anchors.centerIn: parent
                spacing: 10

                Text {
                    visible: root.currentCountryCode() === ""
                    text: "⚡"
                    font.pixelSize: 22
                }

                Image {
                    visible: root.currentCountryCode() !== ""
                    source: visible ? "qrc:/countriesFlags/images/flagKit/" + root.currentCountryCode() + ".svg" : ""
                    sourceSize.width: 28
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 21
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    text: root.currentCountryText()
                    color: NvoStyle.color.paleGray
                    font.pixelSize: 16
                    font.weight: 600
                }

                Text { text: "›"; color: NvoStyle.color.mutedGray; font.pixelSize: 22 }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                enabled: !root.busy && !root.connected
                onClicked: PageController.goToPage(PageEnum.PageNvoCountries)
            }
        }

        // ---- «Плохо подключается?» → Максимальная надёжность (всегда через защищённый канал) ----
        // Спрятано до нужды (§12.3 — одна кнопка): обычным юзерам не мешает, а тем, у кого режут всё,
        // даёт понятный человеческий переключатель без слов «stealth / VLESS / протокол».
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            spacing: 8
            visible: !root.connected && !root.busy

            // Режим «Авто»: ненавязчивая ссылка — предлагаем максимальную надёжность одной строкой.
            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: NvoApi.stealthMode !== 2
                text: qsTr("Плохо подключается? →")
                color: NvoStyle.color.mutedGray
                font.pixelSize: 14
                font.underline: reliMouse.containsMouse
                MouseArea {
                    id: reliMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        NvoApi.stealthMode = 2
                        PageController.showNotificationMessage(
                            qsTr("Максимальная надёжность включена — всегда через защищённый канал"))
                    }
                }
            }

            // Включённый режим максимальной надёжности — спокойная плашка с возможностью вернуть «Авто».
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                visible: NvoApi.stealthMode === 2
                implicitWidth: reliRow.implicitWidth + 32
                implicitHeight: 44
                radius: 22
                color: NvoStyle.color.onyxBlack
                border.width: 1
                border.color: NvoStyle.color.connectedGreenDeep

                RowLayout {
                    id: reliRow
                    anchors.centerIn: parent
                    spacing: 8
                    Text { text: "🛡"; font.pixelSize: 16 }
                    Text {
                        text: qsTr("Максимальная надёжность")
                        color: NvoStyle.color.connectedGreen
                        font.pixelSize: 14
                        font.weight: 600
                    }
                    Text { text: "✕"; color: NvoStyle.color.mutedGray; font.pixelSize: 14 }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        NvoApi.stealthMode = 1
                        PageController.showNotificationMessage(
                            qsTr("Обычный режим — защита включится сама при блокировке"))
                    }
                }
            }
        }
    }

    // ---- Онбординг: одно обучающее окно при первом запуске (§12.8) ----
    Rectangle {
        id: onboarding
        anchors.fill: parent
        z: 100
        visible: NvoApi.isAuthenticated && !NvoApi.onboardingDone
        color: Qt.rgba(0, 0, 0, 0.82)

        MouseArea {
            anchors.fill: parent
            onClicked: NvoApi.setOnboardingDone()
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width
            spacing: 24

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "👆"
                font.pixelSize: 64
            }
            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 32
                Layout.rightMargin: 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Нажмите большую кнопку —\nи вы под защитой")
                color: "white"
                font.family: "PT Root UI VF"
                font.weight: 700
                font.pixelSize: 24
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Нажмите в любом месте, чтобы продолжить")
                color: NvoStyle.color.mutedGray
                font.pixelSize: 15
            }
        }
    }

    // ---- Низ: статус подписки ----
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24 + PageController.safeAreaBottomMargin
        horizontalAlignment: Text.AlignHCenter
        color: NvoStyle.color.mutedGray
        font.pixelSize: 13
        text: {
            if (!NvoApi.hasSubscription)
                return qsTr("Подписка неактивна")
            if (NvoApi.subscriptionDaysRemaining > 0)
                return qsTr("Подписка активна, осталось %1 дн.").arg(NvoApi.subscriptionDaysRemaining)
            return qsTr("Подписка активна")
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: PageController.goToPage(PageEnum.PageNvoSubscription)
        }
    }
}
