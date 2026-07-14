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
        if (connected)
            NvoApi.registerSuccessfulConnection()
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
                NvoApi.connectToSelected()
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

        Item {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 240
            implicitHeight: 240

            // Внешнее кольцо
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: NvoStyle.color.transparent
                border.width: 3
                border.color: root.connected ? NvoStyle.color.connectedGreen
                                              : (root.busy ? NvoStyle.color.nvoBlue
                                                           : NvoStyle.color.charcoalGray)
            }

            // Тело кнопки
            Rectangle {
                id: knob
                anchors.centerIn: parent
                width: 200; height: 200
                radius: width / 2
                // Подключено: зелёная; при наведении — красноватый намёк, что нажатие отключит.
                color: root.connected ? (knobMouse.containsMouse ? "#C7493F" : NvoStyle.color.connectedGreen)
                                      : NvoStyle.color.onyxBlack

                Behavior on color { ColorAnimation { duration: 250 } }
                scale: knobMouse.pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

                // Лого NvoVPN = весь круг кнопки (отключено). Готовый КРУГЛЫЙ PNG (обрезан в круг
                // заранее через PIL) — надёжнее рантайм-масок (OpacityMask/MultiEffect не обрезали).
                Image {
                    anchors.fill: parent
                    source: "qrc:/images/nvoAppIconRound.png"
                    sourceSize.width: 200
                    sourceSize.height: 200
                    fillMode: Image.PreserveAspectFit
                    visible: !root.busy && !root.connected
                }

                // По центру: галочка/крестик (подключено) или спиннер (подключаем).
                Text {
                    anchors.centerIn: parent
                    text: knobMouse.containsMouse ? "✕" : "✓"
                    font.pixelSize: 72
                    color: "white"
                    visible: !root.busy && root.connected
                }
                BusyIndicator {
                    anchors.centerIn: parent
                    running: root.busy
                    visible: root.busy
                    implicitWidth: 64
                    implicitHeight: 64
                }

                MouseArea {
                    id: knobMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: !root.busy
                    hoverEnabled: true
                    onClicked: {
                        if (root.connected) {
                            ConnectionController.closeConnection()
                        } else {
                            NvoApi.connectToSelected()
                        }
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
                                 : (root.busy ? qsTr("Подключаем…")
                                              // iOS (3.1.3(f)): без намёка на платную подписку/триал.
                                              : ((NvoApi.hasSubscription || Qt.platform.os === "ios") ? qsTr("Нажмите, чтобы включить")
                                                                        : qsTr("Подключиться бесплатно (2 дня)")))
            color: root.connected ? NvoStyle.color.connectedGreen : NvoStyle.color.paleGray
            font.family: "PT Root UI VF"
            font.weight: 800
            font.pixelSize: 20
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
