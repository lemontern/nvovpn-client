import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"
import "../Controls2/TextTypes"
import "../Components"

PageType {
    id: root

    BackButtonType {
        id: backButton

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin

        onActiveFocusChanged: {
            if(backButton.enabled && backButton.activeFocus) {
                listView.positionViewAtBeginning()
            }
        }
    }

    ListViewType {
        id: listView

        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.left: parent.left

        header: ColumnLayout {
            width: listView.width

            Image {
                id: image
                source: "qrc:/images/amneziaBigLogo.png"

                Layout.alignment: Qt.AlignCenter
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredWidth: 291
                Layout.preferredHeight: 224
            }

            Header2TextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("О приложении NvoVPN")
                horizontalAlignment: Text.AlignHCenter
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                horizontalAlignment: Text.AlignHCenter

                height: 20
                font.pixelSize: 14

                text: qsTr("NvoVPN защищает ваш интернет — быстро, безопасно и без ограничений.")
                color: AmneziaStyle.color.paleGray
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Contacts")
            }
        }

        model: contacts

        delegate: ColumnLayout {
            width: listView.width

            LabelWithButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 6

                text: title
                descriptionText: description
                leftImageSource: imageSource

                clickedFunction: handler
            }

            DividerType {}

        }

        footer: ColumnLayout {
            width: listView.width

            CaptionTextType {
                Layout.fillWidth: true
                Layout.topMargin: 40

                horizontalAlignment: Text.AlignHCenter

                text: qsTr("Software version: %1").arg(SettingsController.getAppVersion())
                color: AmneziaStyle.color.mutedGray

                MouseArea {
                    property int clickCount: 0
                    anchors.fill: parent
                    onClicked: {
                        // Dev-режим недоступен на iOS: скрытое меню с gateway-URL = риск App Store 2.1.
                        if (Qt.platform.os === "ios")
                            return
                        if (clickCount > 10) {
                            SettingsController.enableDevMode()
                        } else {
                            clickCount++
                        }
                    }
                }
            }

            BasicButtonType {
                id: checkUpdatesButton
                visible: Qt.platform.os !== "ios"   // iOS: обновления через App Store; ссылка вела на сайт

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                Layout.bottomMargin: 16
                implicitHeight: 32

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                disabledColor: AmneziaStyle.color.mutedGray
                textColor: AmneziaStyle.color.goldenApricot

                text: qsTr("Проверить обновления")

                clickedFunc: function() {
                    Qt.openUrlExternally("https://nvovpn.com/")
                }
            }

            BasicButtonType {
                id: privacyPolicyButton

                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 16
                Layout.topMargin: -15
                implicitHeight: 25

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                disabledColor: AmneziaStyle.color.mutedGray
                textColor: AmneziaStyle.color.goldenApricot

                text: qsTr("Privacy Policy")

                clickedFunc: function() {
                    // nvovpn.com/privacy → сайт сам редиректит на нужную локаль (не amnezia.org!)
                    Qt.openUrlExternally("https://nvovpn.com/privacy")
                }
            }
        }
    }
    
    // iOS (App Store 3.1.3(f)): убираем «Сайт … Подписка/оплата» — призыв к покупке вне App Store
    // запрещён. На остальных платформах оставляем как есть.
    property var contacts: Qt.platform.os === "ios" ? [sourceCode] : [website, sourceCode]

    QtObject {
        id: website

        readonly property string title: qsTr("Сайт nvovpn.com")
        readonly property string description: qsTr("Подписка, поддержка, оплата")
        readonly property string imageSource: "qrc:/images/controls/globe-2.svg"
        readonly property var handler: function() {
            Qt.openUrlExternally("https://nvovpn.com/")
        }
    }

    QtObject {
        id: sourceCode

        readonly property string title: qsTr("Исходный код")
        readonly property string description: qsTr("Открытый код (GPLv3)")
        readonly property string imageSource: "qrc:/images/controls/github.svg"
        readonly property var handler: function() {
            Qt.openUrlExternally("https://github.com/lemontern/nvovpn-client")
        }
    }
}
