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

    Component.onCompleted: {
        if (root.isIos)
            NvoApi.fetchIapProducts()   // подтянуть цены StoreKit для кнопок покупки
    }

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
            color: NvoStyle.color.mutedGray
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

        // ---- iOS: покупка подписки через In-App Purchase (App Store 3.1.1) ----
        // На iOS оплата ТОЛЬКО через Apple IAP (иначе отказ 3.1.1). Веб-оплата — на других платформах.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.topMargin: 8
            spacing: 12
            visible: root.isIos && !root.active

            CaptionTextType {
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                color: NvoStyle.color.paleGray
                text: qsTr("Оформить подписку")
            }

            // Год — сверху (выгоднее)
            BasicButtonType {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                enabled: !NvoApi.isBusy && NvoApi.iapPrice1y.length > 0
                text: NvoApi.iapPrice1y.length > 0
                      ? qsTr("1 год — %1").arg(NvoApi.iapPrice1y)
                        + (NvoApi.iapPricePerMonth1y.length > 0 ? qsTr(" (%1/мес)").arg(NvoApi.iapPricePerMonth1y) : "")
                      : qsTr("1 год")
                clickedFunc: function() { NvoApi.purchaseIap("com.nvovpn.app.premium.1y") }
            }

            // Месяц
            BasicButtonType {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                defaultColor: NvoStyle.color.transparent
                hoveredColor: NvoStyle.color.translucentWhite
                pressedColor: NvoStyle.color.sheerWhite
                textColor: NvoStyle.color.paleGray
                borderColor: NvoStyle.color.slateGray
                borderWidth: 1
                enabled: !NvoApi.isBusy && NvoApi.iapPrice1m.length > 0
                text: NvoApi.iapPrice1m.length > 0 ? qsTr("1 месяц — %1").arg(NvoApi.iapPrice1m) : qsTr("1 месяц")
                clickedFunc: function() { NvoApi.purchaseIap("com.nvovpn.app.premium.1m") }
            }

            // Восстановить покупки — обязательный пункт для App Store.
            CaptionTextType {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                horizontalAlignment: Text.AlignHCenter
                color: NvoStyle.color.mutedGray
                text: qsTr("Восстановить покупки")

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: !NvoApi.isBusy
                    onClicked: NvoApi.restoreIap()
                }
            }

            // Обязательная формулировка Apple про авто-продление.
            SmallTextType {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: NvoStyle.color.mutedGray
                font.pixelSize: 11
                text: qsTr("Оплата спишется с вашего Apple ID. Подписка продлевается автоматически, если не отменить её в настройках Apple ID не позднее чем за 24 часа до конца периода.")
            }
        }

        // Баланс (мультивалютный — форматирует бэкенд: «0 ₽» / «0 €»)
        CaptionTextType {
            visible: !root.isIos   // 3.1.3(f): баланс/биллинг не показываем на iOS
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            horizontalAlignment: Text.AlignHCenter
            color: NvoStyle.color.paleGray
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

            defaultColor: NvoStyle.color.transparent
            hoveredColor: NvoStyle.color.translucentWhite
            pressedColor: NvoStyle.color.sheerWhite
            textColor: NvoStyle.color.paleGray
            borderColor: NvoStyle.color.slateGray
            borderWidth: 1

            text: qsTr("Пополнить баланс")
            clickedFunc: function() {
                NvoApi.openWebCabinet("billing")
            }
        }

        // Промокод (кросс-промо «5 дней»): ввод → POST /promo/redeem.
        // СКРЫТО на iOS: App Store Guideline 3.1.1 запрещает разблокировку подписки промокодом
        // в обход In-App Purchase (отказ ревью 2026-07-15). На Win/Android/macOS остаётся.
        CaptionTextType {
            visible: !root.isIos
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            horizontalAlignment: Text.AlignHCenter
            color: NvoStyle.color.paleGray
            text: qsTr("Есть промокод?")
        }

        TextFieldWithHeaderType {
            id: promoField
            visible: !root.isIos
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            textField.placeholderText: qsTr("Например, NVO12345")
        }

        BasicButtonType {
            visible: !root.isIos
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            Layout.preferredHeight: 52
            enabled: !NvoApi.isBusy && promoField.textField.text.trim().length > 0
            text: NvoApi.isBusy ? qsTr("Активируем…") : qsTr("Активировать промокод")
            clickedFunc: function() {
                NvoApi.redeemPromo(promoField.textField.text)
            }
        }
    }

    // Результат активации промокода — тост + очистка поля; при успехе подписка обновится сама.
    Connections {
        target: NvoApi
        function onPromoSucceeded(message, trialDays) {
            promoField.textField.text = ""
            PageController.showNotificationMessage(message)
        }
        function onPromoFailed(message, reason) {
            PageController.showNotificationMessage(message)
        }
        // IAP (iOS): результат покупки/восстановления. При успехе экран сам покажет
        // «Подписка активна» (root.active станет true после refreshUser).
        function onIapPurchaseSucceeded(message) {
            PageController.showNotificationMessage(message)
        }
        function onIapPurchaseFailed(message) {
            PageController.showNotificationMessage(message)
        }
    }
}
