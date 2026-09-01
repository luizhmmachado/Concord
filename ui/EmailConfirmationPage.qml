import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal goToLogin()

    property bool justResent: false

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

            Rectangle {
                width: 64
                height: 64
                radius: 32
                color: Theme.accent
                anchors.horizontalCenter: parent.horizontalCenter
                Text { anchors.centerIn: parent; text: "✉"; font.pixelSize: 28; color: "white" }
            }

            Text {
                text: "Confirme seu e-mail"
                color: Theme.textPrimary
                font.pixelSize: 22
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                text: "Enviamos um link de confirmação para:"
                color: Theme.textSecondary
                font.pixelSize: 13
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                text: authController.pendingConfirmationEmail
                color: Theme.accent
                font.pixelSize: 14
                font.bold: true
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                text: "Clique no link recebido para ativar sua conta e depois volte para entrar."
                color: Theme.textSecondary
                font.pixelSize: 12
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                visible: justResent
                text: "E-mail reenviado com sucesso!"
                color: Theme.online
                font.pixelSize: 12
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
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
                width: parent.width
                text: "Já confirmei, entrar"
                onClicked: goToLogin()
            }

            AppButton {
                width: parent.width
                baseColor: Theme.surfaceAlt
                text: "Reenviar e-mail"
                onClicked: authController.resendConfirmationEmail()
            }
        }
    }

    Connections {
        target: authController
        function onConfirmationEmailResent() { justResent = true }
    }
}
