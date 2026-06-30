import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

PageType {
    id: root

    Connections {
        target: ApiNewsController
        function onFetchNewsFinished() {
            PageController.showBusyIndicator(false)
        }
        
        function onErrorOccurred(errorCode, showError) {
            if (showError) {
                PageController.showErrorMessage(errorCode)
                PageController.closePage()
                PageController.showBusyIndicator(false)
            }
        }
    }

    ListViewType {
        id: listView

        anchors.fill: parent

        header: ColumnLayout {
            width: listView.width

            BaseHeaderType {
                id: header
                Layout.fillWidth: true
                Layout.topMargin: 24 + PageController.safeAreaTopMargin
                Layout.bottomMargin: 16
                Layout.rightMargin: 16
                Layout.leftMargin: 16

                headerText: qsTr("Settings")
            }
        }

        model: settingsEntries

        delegate: ColumnLayout {
            width: listView.width

            spacing: 0

            LabelWithButtonType {
                Layout.fillWidth: true

                visible: isVisible

                text: title
                rightImageSource: "qrc:/images/controls/chevron-right.svg"
                leftImageSource: leftImagePath

                clickedFunction: clickedHandler
            }

            DividerType {
                visible: isVisible
            }
        }

        footer: ColumnLayout {
            width: listView.width

            // NvoVPN: ввести промокод (кросс-промо «5 дней»). Активация бесплатного кода —
            // не покупка, поэтому показываем и на iOS (3.1.3 не нарушается).
            LabelWithButtonType {
                id: promoEntry

                visible: NvoApi.isAuthenticated
                Layout.fillWidth: true

                text: qsTr("Ввести промокод")
                leftImageSource: "qrc:/images/controls/tag.svg"
                isLeftImageHoverEnabled: false

                clickedFunction: function() {
                    PageController.goToPage(PageEnum.PageNvoSubscription)
                }
            }

            DividerType {
                visible: NvoApi.isAuthenticated
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            // NvoVPN: аккаунт + выход
            LabelWithButtonType {
                id: logout

                visible: NvoApi.isAuthenticated
                Layout.fillWidth: true

                text: qsTr("Выйти из аккаунта")
                descriptionText: NvoApi.userEmail
                leftImageSource: "qrc:/images/controls/x-circle.svg"
                isLeftImageHoverEnabled: false

                clickedFunction: function() {
                    if (ConnectionController.isConnected || ConnectionController.isConnectionInProgress) {
                        ConnectionController.closeConnection()
                    }
                    NvoApi.logout()
                }
            }

            DividerType {
                visible: NvoApi.isAuthenticated
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            LabelWithButtonType {
                id: close

                visible: GC.isDesktop()
                Layout.fillWidth: true

                text: qsTr("Close application")
                leftImageSource: "qrc:/images/controls/x-circle.svg"
                isLeftImageHoverEnabled: false

                clickedFunction: function() {
                    PageController.closeApplication()
                }
            }

            DividerType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: GC.isDesktop()
            }
        }
    }

    // NvoVPN: только нужные системные настройки (без ручного управления серверами/бэкапа конфигов).
    property list<QtObject> settingsEntries: [
        connection,
        application,
        news,
        about,
        devConsole
    ]

    QtObject {
        id: connection

        property string title: qsTr("Connection")
        readonly property string leftImagePath: "qrc:/images/controls/radio.svg"
        // NvoVPN: на странице остался только KillSwitch (desktop). DNS/Split Tunneling скрыты.
        // На мобильных страница была бы пустой → показываем пункт лишь на desktop.
        property bool isVisible: GC.isDesktop()
        readonly property var clickedHandler: function() {
            PageController.goToPage(PageEnum.PageSettingsConnection)
        }
    }

    QtObject {
        id: application

        property string title: qsTr("Application")
        readonly property string leftImagePath: "qrc:/images/controls/app.svg"
        property bool isVisible: true
        readonly property var clickedHandler: function() {
            PageController.goToPage(PageEnum.PageSettingsApplication)
        }
    }

    QtObject {
        id: news

        property string title: qsTr("News & Notifications")
        readonly property string leftImagePath: NewsModel.hasUnread && SettingsController.isNewsNotificationsEnabled() ? "qrc:/images/controls/news-unread.svg" : "qrc:/images/controls/news.svg"
        property bool isVisible: ServersUiController.hasServersFromGatewayApi
        readonly property var clickedHandler: function() {
            if (!ServersUiController.hasServersFromGatewayApi) {
                return;
            }
            PageController.showBusyIndicator(true)
            ApiNewsController.fetchNews(true)
            PageController.goToPage(PageEnum.PageSettingsNewsNotifications)
        }
    }

    QtObject {
        id: about

        property string title: qsTr("О NvoVPN")
        readonly property string leftImagePath: "qrc:/images/controls/globe-2.svg"
        property bool isVisible: true
        readonly property var clickedHandler: function() {
            PageController.goToPage(PageEnum.PageSettingsAbout)
        }
    }

    QtObject {
        id: devConsole

        property string title: qsTr("Dev console")
        readonly property string leftImagePath: "qrc:/images/controls/bug.svg"
        // Скрыто на iOS — dev-консоль показывает gateway-endpoint (инфра), риск App Store 2.1.
        property bool isVisible: SettingsController.isDevModeEnabled && Qt.platform.os !== "ios"
        readonly property var clickedHandler: function() {
            PageController.goToPage(PageEnum.PageDevMenu)
        }
    }
}
