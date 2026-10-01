import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Config"

// NvoVPN: технические экраны Amnezia скрыты (ТЗ §12 — простой UX без жаргона):
// AmneziaDNS, DNS-серверы, Site/App split tunneling убраны. Остался только KillSwitch (desktop).
// Сама страница показывается лишь на desktop (см. PageSettings: connection.isVisible = GC.isDesktop()).
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
        anchors.left: parent.left
        anchors.right: parent.right

        header: ColumnLayout {

            width: listView.width

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Connection")
            }
        }

        model: 1 // fake model to force the ListView to be created without a model

        delegate: ColumnLayout {

            width: listView.width

            LabelWithButtonType {
                id: killSwitchButton
                // 01.10.2026 (аудит M-10, D-4): на Android кнопка была скрыта — страница «Connection» открывалась пустой,
                // а инструкция про системный Always-on была недостижима. На macOS (сборка с сетевым расширением)
                // KillSwitch не реализован — не показываем, чтобы не обещать защиту, которой нет.
                visible: Qt.platform.os === "android" || (GC.isDesktop() && !IsMacOsNeBuild)

                Layout.fillWidth: true

                text: qsTr("KillSwitch")
                descriptionText: Qt.platform.os === "android"
                                 ? qsTr("Блокировка интернета без VPN — через системную настройку «Постоянная VPN»")
                                 : qsTr("Blocks network connections without VPN")
                rightImageSource: "qrc:/images/controls/chevron-right.svg"

                clickedFunction: function() {
                    PageController.goToPage(PageEnum.PageSettingsKillSwitch)
                }
            }

            DividerType {
                visible: killSwitchButton.visible
            }
        }
    }
}
