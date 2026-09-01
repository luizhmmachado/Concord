import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal goToRegister()

    Column {
        anchors.centerIn: parent
        width: 340
        spacing: 16

        Text {
            text: "Entrar no Concord"
            color: Theme.textPrimary
            font.pixelSize: 26
            font.bold: true
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
        }

        AppTextField {
            id: emailField
            width: parent.width
            placeholderText: "E-mail"
        }

        AppTextField {
            id: passwordField
            width: parent.width
            placeholderText: "Senha"
            echoMode: TextInput.Password
        }

        Text {
            visible: authController.lastError.length > 0
            text: authController.lastError
            color: Theme.danger
            width: parent.width
            wrapMode: Text.WordWrap
        }

        AppButton {
            width: parent.width
            text: authController.busy ? "Entrando..." : "Entrar"
            enabled: !authController.busy && emailField.text.length > 0 && passwordField.text.length > 0
            onClicked: authController.signIn(emailField.text, passwordField.text)
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: "<a href='#'>Criar uma conta</a>"
            color: Theme.textSecondary
            onLinkActivated: goToRegister()

            MouseArea {
                anchors.fill: parent
                onClicked: goToRegister()
            }
        }
    }
}
