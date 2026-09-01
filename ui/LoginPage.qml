import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal goToRegister()
    signal emailUnconfirmed()

    Connections {
        target: authController
        function onEmailNotConfirmed() { emailUnconfirmed() }
    }

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
            spacing: 18

            Text {
                text: "Bem-vindo de volta"
                color: Theme.textPrimary
                font.pixelSize: 24
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                text: "Entre com seu e-mail ou usuário"
                color: Theme.textSecondary
                font.pixelSize: 13
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "EMAIL OU USUÁRIO"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField {
                    id: identifierField
                    width: parent.width
                    placeholderText: "voce@exemplo.com ou usuario"
                }
            }

            Column {
                width: parent.width
                spacing: 6
                Text { text: "SENHA"; color: Theme.textSecondary; font.pixelSize: 11; font.bold: true }
                AppTextField {
                    id: passwordField
                    width: parent.width
                    placeholderText: "Sua senha"
                    echoMode: TextInput.Password
                    onAccepted: loginButton.clicked()
                }
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
                id: loginButton
                width: parent.width
                text: authController.busy ? "Entrando..." : "Entrar"
                enabled: !authController.busy && identifierField.text.length > 0 && passwordField.text.length > 0
                onClicked: authController.signIn(identifierField.text, passwordField.text)
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "Não tem uma conta? <b>Criar conta</b>"
                color: Theme.textSecondary
                font.pixelSize: 13

                MouseArea {
                    anchors.fill: parent
                    onClicked: goToRegister()
                }
            }
        }
    }
}
