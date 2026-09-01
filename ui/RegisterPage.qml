import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal goToLogin()

    Column {
        anchors.centerIn: parent
        width: 340
        spacing: 16

        Text {
            text: "Criar conta"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
        }

        AppTextField { id: emailField; width: parent.width; placeholderText: "E-mail" }
        AppTextField { id: usernameField; width: parent.width; placeholderText: "Nome de usuário" }
        AppTextField { id: displayNameField; width: parent.width; placeholderText: "Nome de exibição" }
        AppTextField { id: passwordField; width: parent.width; placeholderText: "Senha"; echoMode: TextInput.Password }

        Text {
            visible: authController.lastError.length > 0
            text: authController.lastError
            color: Theme.danger
            width: parent.width
            wrapMode: Text.WordWrap
        }

        AppButton {
            width: parent.width
            text: authController.busy ? "Criando..." : "Criar conta"
            enabled: !authController.busy && emailField.text.length > 0 && passwordField.text.length > 0 && usernameField.text.length > 0
            onClicked: authController.signUp(emailField.text, passwordField.text, usernameField.text, displayNameField.text)
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "Já tem conta? Entrar"
            color: Theme.textSecondary

            MouseArea {
                anchors.fill: parent
                onClicked: goToLogin()
            }
        }
    }

    Connections {
        target: authController
        function onSignUpSucceeded() { goToLogin() }
    }
}
