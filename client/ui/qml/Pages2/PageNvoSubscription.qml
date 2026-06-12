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

// Подписка (ТЗ §12.5): активна — просто статус; закончилась — дружелюбно «Продлите», не тупик.
PageType {
    id: root

    readonly property bool active: NvoApi.hasSubscription

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width
        spacing: 16

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: root.active ? "✅" : "🔒"
            font.pixelSize: 64
        }

        Header2TextType {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            horizontalAlignment: Text.AlignHCenter
            text: root.active ? qsTr("Подписка активна") : qsTr("Подписка закончилась")
        }

        ParagraphTextType {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            horizontalAlignment: Text.AlignHCenter
            color: AmneziaStyle.color.mutedGray
            wrapMode: Text.WordWrap
            text: {
                if (root.active && NvoApi.subscriptionDaysRemaining > 0)
                    return qsTr("План «%1». Осталось %2 дн.").arg(NvoApi.subscriptionPlan).arg(NvoApi.subscriptionDaysRemaining)
                if (root.active)
                    return qsTr("План «%1»").arg(NvoApi.subscriptionPlan)
                return qsTr("Продлите подписку на сайте, чтобы снова пользоваться защитой. Это займёт минуту.")
            }
        }

        BasicButtonType {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.preferredHeight: 56
            text: root.active ? qsTr("Управление подпиской") : qsTr("Продлить подписку")
            clickedFunc: function() {
                Qt.openUrlExternally("https://nvovpn.com/")
            }
        }
    }
}
