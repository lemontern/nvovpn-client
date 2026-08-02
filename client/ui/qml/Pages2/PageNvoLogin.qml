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
    property bool showPassword: false

    // iOS App Store, правило 3.1.3(f): companion-приложение к платному веб-сервису не должно
    // содержать регистрацию/сторонние входы/ссылки на оплату. Поэтому на iOS — ТОЛЬКО email+пароль.
    // (Нет сторонних соц-входов → Sign in with Apple по правилу 4.8 не требуется.)
    readonly property bool isIos: Qt.platform.os === "ios"
    // Google-вход через polling работает на всех платформах, КРОМЕ iOS (3.1.3(f)).
    readonly property bool googleAvailable: !isIos
    // Sign in with Apple — тоже скрыт на iOS (там вообще нет соц-входов, чистый email+пароль).
    readonly property bool appleAvailable: !isIos

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
                        text: qsTr("Безопасный интернет")
                        color: NvoStyle.color.mutedGray
                        font.pixelSize: 15
                    }
                }
            }

            // ---- Email + пароль ----
            TextFieldWithHeaderType {
                id: emailField
                visible: !root.codeMode
                Layout.fillWidth: true
                Layout.topMargin: 40
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
                textField.placeholderText: qsTr("Пароль")

                Component.onCompleted: passwordField.textField.echoMode = TextInput.Password
            }

            CaptionTextType {
                visible: !root.codeMode
                Layout.leftMargin: 24
                Layout.topMargin: 8
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
                text: NvoApi.isBusy ? qsTr("Входим…") : qsTr("Войти")

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
                    } else {
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
                        PageController.showBusyIndicator(true)
                        NvoApi.login(email, pwd)
                    }
                }
            }

            // ---- «или» + вход через Google (только мобильные, см. root.googleAvailable) ----
            CaptionTextType {
                visible: !root.codeMode && root.googleAvailable
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                color: NvoStyle.color.mutedGray
                text: qsTr("или")
            }

            BasicButtonType {
                id: googleButton
                visible: !root.codeMode && root.googleAvailable
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
                text: NvoApi.isBusy ? qsTr("Ожидаем вход через Google…") : qsTr("Войти через Google")

                clickedFunc: function() {
                    errorLabel.text = ""
                    NvoApi.loginWithGoogle()
                }
            }

            // ---- Sign in with Apple (обязателен для App Store при наличии соц-входов) ----
            BasicButtonType {
                id: appleButton
                visible: !root.codeMode && root.appleAvailable
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
                text: NvoApi.isBusy ? qsTr("Ожидаем вход через Apple…") : qsTr("Войти через Apple")

                clickedFunc: function() {
                    errorLabel.text = ""
                    NvoApi.loginWithApple()
                }
            }

            // ---- Переключатель режима ----
            // App Store 2.1: на iOS вход по коду СКРЫТ — только email+пароль (Apple спрашивала
            // «как получают код / платный ли он»). Переключатель — единственный вход в codeMode,
            // поэтому его скрытие полностью убирает код-логин из iOS-сборки.
            CaptionTextType {
                visible: !root.isIos
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
                // 3.1.3(f): на iOS НЕ показываем регистрацию/ссылку на сайт (там же и оплата).
                visible: !root.isIos
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 32
                Layout.bottomMargin: 24
                spacing: 6

                CaptionTextType {
                    color: NvoStyle.color.mutedGray
                    text: qsTr("Нет аккаунта?")
                }

                CaptionTextType {
                    color: NvoStyle.color.nvoBlue
                    font.weight: 700
                    text: qsTr("Зарегистрироваться")

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://nvovpn.com/")
                    }
                }
            }
        }
    }
}
