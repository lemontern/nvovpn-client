import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

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

    function currentCountryText() {
        if (NvoApi.selectedServerId < 0)
            return qsTr("Авто (лучший сервер)")
        var idx = NvoServersModel.indexOfServerId(NvoApi.selectedServerId)
        var name = NvoServersModel.nameAt(idx)
        return name === "" ? qsTr("Авто (лучший сервер)") : name
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
            color: AmneziaStyle.color.paleGray
            font.family: "PT Root UI VF"
            font.weight: 800
            font.pixelSize: 20
        }

        Item { Layout.fillWidth: true }

        ImageButtonType {
            image: "qrc:/images/controls/settings.svg"
            imageColor: AmneziaStyle.color.paleGray
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
                color: AmneziaStyle.color.transparent
                border.width: 3
                border.color: root.connected ? AmneziaStyle.color.connectedGreen
                                              : (root.busy ? AmneziaStyle.color.nvoBlue
                                                           : AmneziaStyle.color.charcoalGray)
            }

            // Тело кнопки
            Rectangle {
                id: knob
                anchors.centerIn: parent
                width: 200; height: 200
                radius: width / 2
                // Подключено: зелёная; при наведении — красноватый намёк, что нажатие отключит.
                color: root.connected ? (knobMouse.containsMouse ? "#C7493F" : AmneziaStyle.color.connectedGreen)
                                      : AmneziaStyle.color.onyxBlack

                Behavior on color { ColorAnimation { duration: 250 } }
                scale: knobMouse.pressed ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }

                // Лого NvoVPN = весь круг кнопки (отключено), обрезано в идеальный круг через
                // MultiEffect (нативный Qt6, надёжнее устаревшего OpacityMask из Qt5Compat).
                Image {
                    id: logoImg
                    anchors.fill: parent
                    source: "qrc:/images/nvoAppIcon.png"
                    sourceSize.width: 200
                    sourceSize.height: 200
                    fillMode: Image.PreserveAspectCrop
                    visible: false
                }
                Item {
                    id: circleMask
                    anchors.fill: parent
                    layer.enabled: true
                    visible: false
                    Rectangle { anchors.fill: parent; radius: width / 2; color: "black" }
                }
                MultiEffect {
                    anchors.fill: parent
                    source: logoImg
                    maskEnabled: true
                    maskSource: circleMask
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
                                              : (NvoApi.hasSubscription ? qsTr("Нажмите, чтобы включить")
                                                                        : qsTr("Подключиться бесплатно (2 дня)")))
            color: root.connected ? AmneziaStyle.color.connectedGreen : AmneziaStyle.color.paleGray
            font.family: "PT Root UI VF"
            font.weight: 800
            font.pixelSize: 20
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Ваш интернет защищён, IP скрыт")
            color: AmneziaStyle.color.mutedGray
            font.pixelSize: 13
            visible: root.connected
        }

        // ---- Выбор страны ----
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            implicitWidth: countryRow.implicitWidth + 40
            implicitHeight: 56
            radius: 28
            color: AmneziaStyle.color.onyxBlack
            border.width: 1
            border.color: AmneziaStyle.color.slateGray

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
                    color: AmneziaStyle.color.paleGray
                    font.pixelSize: 16
                    font.weight: 600
                }

                Text { text: "›"; color: AmneziaStyle.color.mutedGray; font.pixelSize: 22 }
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
                color: AmneziaStyle.color.mutedGray
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
        color: AmneziaStyle.color.mutedGray
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
