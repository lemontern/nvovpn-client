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

    // Google-вход через polling (без deep-link) работает на всех платформах:
    // браузер открывается, приложение опрашивает /auth/poll и получает токен в фоне.
    readonly property bool googleAvailable: true
    // Sign in with Apple — через тот же polling (/app/login/apple). На iOS обязателен при соц-входах.
    readonly property bool appleAvailable: true

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

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 72; height: 72; radius: 18
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: AmneziaStyle.color.nvoGradientStart }
                            GradientStop { position: 1.0; color: AmneziaStyle.color.nvoGradientEnd }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "🛡"
                            font.pixelSize: 36
                            color: "white"
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "NvoVPN"
                        color: AmneziaStyle.color.paleGray
                        font.family: "PT Root UI VF"
                        font.weight: 800
                        font.pixelSize: 28
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Безопасный интернет")
                        color: AmneziaStyle.color.mutedGray
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
                color: AmneziaStyle.color.nvoBlue
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
                color: AmneziaStyle.color.vibrantRed
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
                color: AmneziaStyle.color.mutedGray
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

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderColor: AmneziaStyle.color.slateGray
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

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderColor: AmneziaStyle.color.slateGray
                borderWidth: 1

                enabled: !NvoApi.isBusy
                text: NvoApi.isBusy ? qsTr("Ожидаем вход через Apple…") : qsTr("Войти через Apple")

                clickedFunc: function() {
                    errorLabel.text = ""
                    NvoApi.loginWithApple()
                }
            }

            // ---- Переключатель режима ----
            CaptionTextType {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20
                horizontalAlignment: Text.AlignHCenter
                color: AmneziaStyle.color.nvoBlue
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
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 32
                Layout.bottomMargin: 24
                spacing: 6

                CaptionTextType {
                    color: AmneziaStyle.color.mutedGray
                    text: qsTr("Нет аккаунта?")
                }

                CaptionTextType {
                    color: AmneziaStyle.color.nvoBlue
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
