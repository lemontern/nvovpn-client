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
                        text: qsTr("Сервер подберётся автоматически")
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
            // Не притемняем по health_status — он бывает ложным (France: unhealthy, но работает).
            opacity: 1.0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 14
                z: 1   // выше row-MouseArea, чтобы клик по звезде ловился отдельно от выбора строки

                Image {
                    source: "qrc:/countriesFlags/images/flagKit/" + countryImageCode + ".svg"
                    sourceSize.width: 36
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 27
                    fillMode: Image.PreserveAspectFit
                }

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

                // Звезда «в избранное» — переключатель, сохраняется между запусками.
                Text {
                    readonly property bool fav: NvoApi.favoriteCountries.indexOf(countryImageCode.toUpperCase()) >= 0
                    text: fav ? "★" : "☆"
                    color: fav ? AmneziaStyle.color.nvoBlue : AmneziaStyle.color.slateGray
                    font.pixelSize: 22
                    Layout.preferredWidth: 30
                    horizontalAlignment: Text.AlignHCenter
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: NvoApi.toggleFavoriteCountry(countryImageCode)
                    }
                }

                Text {
                    text: NvoApi.selectedServerId === serverId ? "✓" : ""
                    color: AmneziaStyle.color.nvoBlue
                    font.pixelSize: 22
                }
            }

            MouseArea {
                anchors.fill: parent
                // Выбор разрешён даже для нод с health_status=unhealthy: бэкендовый healthcheck
                // бывает ложным (France: unhealthy, но /connect 200 и нода реально работает).
                // Притемнение (opacity) остаётся как подсказка; реальный обрыв обработает failover.
                enabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selectAndBack(serverId)
            }
        }
    }
}
