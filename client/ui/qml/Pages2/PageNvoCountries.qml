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

// Выбор страны (ТЗ §12.4): крупно, сверху «Авто». Без пингов/технических цифр.
PageType {
    id: root

    function selectAndBack(serverId) {
        NvoApi.setSelectedServerId(serverId)
        PageController.closePage()
    }

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    Header2TextType {
        id: header
        anchors.top: backButton.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        anchors.topMargin: 8
        text: qsTr("Выберите страну")
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        clip: true
        spacing: 12

        model: NvoServersModel

        header: Rectangle {
            width: list.width
            height: 72
            radius: 16
            color: AmneziaStyle.color.onyxBlack
            border.width: NvoApi.selectedServerId < 0 ? 2 : 1
            border.color: NvoApi.selectedServerId < 0 ? AmneziaStyle.color.nvoBlue : AmneziaStyle.color.slateGray

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 14

                Text { text: "⚡"; font.pixelSize: 28 }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("Авто")
                        color: AmneziaStyle.color.paleGray
                        font.pixelSize: 18; font.weight: 700
                    }
                    Text {
                        text: qsTr("Лучший сервер автоматически")
                        color: AmneziaStyle.color.mutedGray
                        font.pixelSize: 13
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: NvoApi.selectedServerId < 0 ? "✓" : ""
                    color: AmneziaStyle.color.nvoBlue
                    font.pixelSize: 22
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selectAndBack(-1)
            }
        }

        delegate: Rectangle {
            width: list.width
            height: 72
            radius: 16
            color: AmneziaStyle.color.onyxBlack
            border.width: NvoApi.selectedServerId === serverId ? 2 : 1
            border.color: NvoApi.selectedServerId === serverId ? AmneziaStyle.color.nvoBlue : AmneziaStyle.color.slateGray
            opacity: online ? 1.0 : 0.45

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 14

                Text { text: "🌍"; font.pixelSize: 26 }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: name
                        color: AmneziaStyle.color.paleGray
                        font.pixelSize: 18; font.weight: 700
                    }
                    Text {
                        text: city + (recommended ? qsTr(" · рекомендуем") : "")
                        color: recommended ? AmneziaStyle.color.connectedGreen : AmneziaStyle.color.mutedGray
                        font.pixelSize: 13
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: NvoApi.selectedServerId === serverId ? "✓" : ""
                    color: AmneziaStyle.color.nvoBlue
                    font.pixelSize: 22
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: online
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selectAndBack(serverId)
            }
        }
    }
}
