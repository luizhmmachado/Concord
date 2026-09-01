import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    ListView {
        id: messageList
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: inputRow.top
        anchors.margins: 12
        spacing: 10
        model: chatController.messages
        clip: true

        delegate: Row {
            width: messageList.width
            spacing: 10

            Avatar { displayName: authorName }

            Column {
                width: parent.width - 50
                Row {
                    spacing: 6
                    Text { text: authorName; color: Theme.textPrimary; font.bold: true }
                    Text { text: createdAt; color: Theme.textSecondary; font.pixelSize: 11 }
                }
                Text {
                    text: content
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }

        onCountChanged: positionViewAtEnd()
    }

    Row {
        id: inputRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        spacing: 8

        AppTextField {
            id: messageField
            width: parent.width - sendButton.width - 8
            placeholderText: "Enviar mensagem..."
            onAccepted: sendButton.clicked()
        }

        AppButton {
            id: sendButton
            text: "Enviar"
            onClicked: {
                if (messageField.text.length === 0)
                    return
                chatController.sendMessage(messageField.text)
                messageField.text = ""
            }
        }
    }
}
