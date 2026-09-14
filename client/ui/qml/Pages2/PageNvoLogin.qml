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

// Стартовый экран NvoVPN — вход без боли (ТЗ §12.1).
// Главный путь: email + пароль (крупные поля, «показать пароль»). Альтернатива: короткий код.
PageType {
    id: root

    property bool codeMode: false
    property bool registerMode: false
    property bool showPassword: false

    readonly property bool isIos: Qt.platform.os === "ios"
    // 14.09.2026: приложение одобрено App Store (1.0) и продаёт подписку через In-App Purchase —
    // правило 3.1.3(f) о «companion-приложении без входов и регистрации» больше не применяется.
    // Google-вход и Sign in with Apple включены на всех платформах (4.8: раз есть Google — нужен и Apple).
    // Без них люди с аккаунтами, заведёнными на сайте через Google/Apple (больше половины базы),
    // в iOS-версии не могли войти вообще: пароля у таких аккаунтов нет.
    readonly property bool googleAvailable: true
    readonly property bool appleAvailable: true
    // Регистрация: на iOS — внутри приложения (ссылка на сайт с ценами = риск App Store 3.1.1),
    // на остальных платформах — как раньше, на сайте.
    readonly property bool inAppRegister: isIos

    Connections {
        target: NvoApi

        function onLoginSucceeded() {
            PageController.showBusyIndicator(false)
            PageController.goToPageHome()
        }

        function onLoginFailed(message) {
            PageController.showBusyIndicator(false)
            errorLabel.text = message
        }
    }

    FlickableType {
        id: flick
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        contentHeight: content.implicitHeight + 48

        ColumnLayout {
            id: content
            width: flick.width
            spacing: 0

            // ---- Логотип / название ----
            Item {
                Layout.fillWidth: true
                Layout.topMargin: 64 + PageController.safeAreaTopMargin
                Layout.preferredHeight: 96

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        source: "qrc:/images/nvoAppIcon.png"
                        sourceSize.width: 88
                        sourceSize.height: 88
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "NvoVPN"
                        color: NvoStyle.color.paleGray
                        font.family: "PT Root UI VF"
                        font.weight: 800
                        font.pixelSize: 28
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.registerMode ? qsTr("Создание аккаунта") : qsTr("Безопасный интернет")
                        color: NvoStyle.color.mutedGray
                        font.pixelSize: 15
                    }
                }
            }

            // ---- Имя (только регистрация) ----
            TextFieldWithHeaderType {
                id: nameField
                visible: root.registerMode
                Layout.fillWidth: true
                Layout.topMargin: 40
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                headerText: qsTr("Имя")
                textField.placeholderText: qsTr("Как к вам обращаться")
                textField.inputMethodHints: Qt.ImhNoPredictiveText
            }

            // ---- Email + пароль ----
            TextFieldWithHeaderType {
                id: emailField
                visible: !root.codeMode
                Layout.fillWidth: true
                Layout.topMargin: root.registerMode ? 16 : 40
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                headerText: qsTr("Электронная почта")
                textField.placeholderText: qsTr("Email")
                textField.inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            }

            TextFieldWithHeaderType {
                id: passwordField
                visible: !root.codeMode
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                headerText: qsTr("Пароль")
                textField.placeholderText: root.registerMode ? qsTr("Не короче 8 символов") : qsTr("Пароль")

                Component.onCompleted: passwordField.textField.echoMode = TextInput.Password
            }

            RowLayout {
                visible: !root.codeMode
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 8

                CaptionTextType {
                    color: NvoStyle.color.nvoBlue
                    text: root.showPassword ? qsTr("Скрыть пароль") : qsTr("Показать пароль")

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.showPassword = !root.showPassword
                            passwordField.textField.echoMode = root.showPassword ? TextInput.Normal : TextInput.Password
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Восстановление пароля — страница сайта (через активный домен). Нужна и тем, кто
                // регистрировался через Google/Apple и хочет завести обычный пароль.
                CaptionTextType {
                    visible: !root.registerMode
                    color: NvoStyle.color.nvoBlue
                    text: qsTr("Забыли пароль?")

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: NvoApi.openForgotPassword()
                    }
                }
            }

            // ---- Вход по коду ----
            TextFieldWithHeaderType {
                id: codeField
                visible: root.codeMode
                Layout.fillWidth: true
                Layout.topMargin: 40
                Layout.leftMargin: 24
                Layout.rightMargin: 24

                headerText: qsTr("Код из личного кабинета")
                textField.placeholderText: qsTr("Например, 482913")
                textField.inputMethodHints: Qt.ImhDigitsOnly
            }

            // ---- Ошибка ----
            CaptionTextType {
                id: errorLabel
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 16
                color: NvoStyle.color.vibrantRed
                wrapMode: Text.WordWrap
                visible: text !== ""
            }

            // ---- Главная кнопка ----
            BasicButtonType {
                id: loginButton
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.preferredHeight: 56

                enabled: !NvoApi.isBusy
                text: {
                    if (root.registerMode)
                        return NvoApi.isBusy ? qsTr("Создаём аккаунт…") : qsTr("Создать аккаунт")
                    return NvoApi.isBusy ? qsTr("Входим…") : qsTr("Войти")
                }

                clickedFunc: function() {
                    errorLabel.text = ""
                    if (root.codeMode) {
                        var code = codeField.textField.text.trim()
                        if (code.length === 0) {
                            errorLabel.text = qsTr("Введите код")
                            return
                        }
                        PageController.showBusyIndicator(true)
                        NvoApi.loginByCode(code)
                        return
                    }
                    var email = emailField.textField.text.trim()
                    var pwd = passwordField.textField.text
                    if (email.indexOf("@") < 0) {
                        errorLabel.text = qsTr("Введите корректный email")
                        return
                    }
                    if (pwd.length === 0) {
                        errorLabel.text = qsTr("Введите пароль")
                        return
                    }
                    if (root.registerMode) {
                        var name = nameField.textField.text.trim()
                        if (name.length === 0) {
                            errorLabel.text = qsTr("Введите имя")
                            return
                        }
                        if (pwd.length < 8) {
                            errorLabel.text = qsTr("Пароль должен быть не короче 8 символов")
                            return
                        }
                        PageController.showBusyIndicator(true)
                        NvoApi.registerAccount(name, email, pwd)
                        return
                    }
                    PageController.showBusyIndicator(true)
                    NvoApi.login(email, pwd)
                }
            }

            // Подсказка при регистрации: что будет дальше (письмо подтверждения → пробный период).
            CaptionTextType {
                visible: root.registerMode
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.topMargin: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: NvoStyle.color.mutedGray
                text: qsTr("На почту придёт письмо для подтверждения адреса — после него откроется доступ.")
            }

            // ---- «или» + вход через Apple / Google ----
            CaptionTextType {
                visible: !root.codeMode && (root.googleAvailable || root.appleAvailable)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                color: NvoStyle.color.mutedGray
                text: qsTr("или")
            }

            // Порядок: на iOS первым Sign in with Apple (App Store 4.8 / HIG), на остальных — Google.
            Repeater {
                model: root.isIos ? ["apple", "google"] : ["google", "apple"]

                delegate: BasicButtonType {
                    required property string modelData
                    readonly property bool isApple: modelData === "apple"

                    visible: !root.codeMode && (isApple ? root.appleAvailable : root.googleAvailable)
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

                    enabled: !NvoApi.isBusy
                    text: {
                        if (isApple)
                            return NvoApi.isBusy ? qsTr("Ожидаем вход через Apple…") : qsTr("Войти через Apple")
                        return NvoApi.isBusy ? qsTr("Ожидаем вход через Google…") : qsTr("Войти через Google")
                    }

                    clickedFunc: function() {
                        errorLabel.text = ""
                        if (isApple)
                            NvoApi.loginWithApple()
                        else
                            NvoApi.loginWithGoogle()
                    }
                }
            }

            // ---- Переключатель режима ----
            // App Store 2.1: на iOS вход по коду СКРЫТ — только email+пароль и соц-входы (Apple спрашивала
            // «как получают код / платный ли он»). Переключатель — единственный вход в codeMode,
            // поэтому его скрытие полностью убирает код-логин из iOS-сборки.
            CaptionTextType {
                visible: !root.isIos && !root.registerMode
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20
                horizontalAlignment: Text.AlignHCenter
                color: NvoStyle.color.nvoBlue
                text: root.codeMode ? qsTr("Войти по email и паролю") : qsTr("У меня есть код для входа")

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        errorLabel.text = ""
                        root.codeMode = !root.codeMode
                    }
                }
            }

            RowLayout {
                visible: !root.codeMode
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 32
                Layout.bottomMargin: 24
                spacing: 6

                CaptionTextType {
                    color: NvoStyle.color.mutedGray
                    text: root.registerMode ? qsTr("Уже есть аккаунт?") : qsTr("Нет аккаунта?")
                }

                CaptionTextType {
                    color: NvoStyle.color.nvoBlue
                    font.weight: 700
                    text: root.registerMode ? qsTr("Войти") : qsTr("Зарегистрироваться")

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            errorLabel.text = ""
                            if (root.registerMode) {
                                root.registerMode = false
                            } else if (root.inAppRegister) {
                                root.registerMode = true
                            } else {
                                Qt.openUrlExternally("https://nvovpn.com/")
                            }
                        }
                    }
                }
            }
        }
    }
}
