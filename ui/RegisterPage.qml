import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal goToLogin()
    signal registered()

    Rectangle {
        anchors.centerIn: parent
        width: 380
        height: card.implicitHeight + 64
        radius: Theme.radiusMedium
        color: Theme.surface
        border.width: 1
        border.color: Theme.surfaceAlt

        Column {
            id: card
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16

            Text {
                text: "Criar conta"
                color: Theme.textPrimary
                font.pixelSize: 24
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "Preencha os dados para começar"
                color: Theme.textSecondary
                font.pixelSize: 13
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "NOME DE USUÁRIO"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField { id: usernameField; width: parent.width; placeholderText: "usuario" }
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "EMAIL"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField { id: emailField; width: parent.width; placeholderText: "voce@exemplo.com" }
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "SENHA"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField { id: passwordField; width: parent.width; placeholderText: "Mínimo 6 caracteres"; echoMode: TextInput.Password }
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "CONFIRMAR SENHA"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField {
                    id: confirmPasswordField
                    width: parent.width
                    placeholderText: "Repita a senha"
                    echoMode: TextInput.Password
                    onAccepted: registerButton.clicked()
                }
            }

            Text {
                visible: confirmPasswordField.text.length > 0 && confirmPasswordField.text !== passwordField.text
                text: "As senhas não coincidem"
                color: Theme.danger
                font.pixelSize: 12
                width: parent.width
            }

            Text {
                visible: authController.lastError.length > 0
                text: authController.lastError
                color: Theme.danger
                font.pixelSize: 12
                width: parent.width
                wrapMode: Text.WordWrap
            }

            AppButton {
                id: registerButton
                width: parent.width
                text: authController.busy ? "Criando..." : "Criar conta"
                enabled: !authController.busy
                    && emailField.text.length > 0
                    && usernameField.text.length > 0
                    && passwordField.text.length >= 6
                    && passwordField.text === confirmPasswordField.text
                onClicked: authController.signUp(emailField.text, passwordField.text, usernameField.text, usernameField.text)
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "Já tem conta? <b>Entrar</b>"
                color: Theme.textSecondary
                font.pixelSize: 13

                MouseArea {
                    anchors.fill: parent
                    onClicked: goToLogin()
                }
            }
        }
    }

    Connections {
        target: authController
        function onSignUpSucceeded() { registered() }
    }
}
