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
    // iOS App Store 3.1.3(f): никаких цен/кнопок/ссылок «купить/продлить/пополнить» внутри iOS.
    // На iOS показываем ТОЛЬКО нейтральный статус подписки, без CTA на оплату и без слова «сайт».
    readonly property bool isIos: Qt.platform.os === "ios"

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
                // iOS (3.1.3(f)): нейтрально, без CTA на оплату и без упоминания сайта.
                if (root.isIos)
                    return qsTr("Сейчас нет активной подписки.")
                return qsTr("Продлите подписку на сайте, чтобы снова пользоваться защитой. Это займёт минуту.")
            }
        }

        // Баланс (мультивалютный — форматирует бэкенд: «0 ₽» / «0 €»)
        CaptionTextType {
            visible: !root.isIos   // 3.1.3(f): баланс/биллинг не показываем на iOS
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            horizontalAlignment: Text.AlignHCenter
            color: AmneziaStyle.color.paleGray
            text: qsTr("Баланс: %1").arg(NvoApi.balanceFormatted !== "" ? NvoApi.balanceFormatted : "—")
        }

        // Основная кнопка: продлить (истекла) / управление (активна) — открывает залогиненный ЛК.
        // 3.1.3(f): на iOS скрыта (ведёт на оплату в вебе).
        BasicButtonType {
            visible: !root.isIos
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.preferredHeight: 56
            text: root.active ? qsTr("Управление подпиской") : qsTr("Продлить подписку")
            clickedFunc: function() {
                NvoApi.openWebCabinet(root.active ? "" : "plans")
            }
        }

        // Пополнить баланс — для авто-продления подписки с баланса (открывает страницу оплаты в ЛК).
        // 3.1.3(f): на iOS скрыта (прямой CTA на оплату).
        BasicButtonType {
            visible: !root.isIos
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.preferredHeight: 56

            defaultColor: AmneziaStyle.color.transparent
            hoveredColor: AmneziaStyle.color.translucentWhite
            pressedColor: AmneziaStyle.color.sheerWhite
            textColor: AmneziaStyle.color.paleGray
            borderColor: AmneziaStyle.color.slateGray
            borderWidth: 1

            text: qsTr("Пополнить баланс")
            clickedFunc: function() {
                NvoApi.openWebCabinet("billing")
            }
        }
    }
}
