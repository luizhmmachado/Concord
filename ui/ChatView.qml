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
        spacing: 4
        model: chatController.messages
        clip: true

        delegate: Column {
            width: messageList.width
            spacing: 4

            Item { width: 1; height: grouped ? 0 : 8 }

            Item {
                width: parent.width
                height: daySeparator.length > 0 ? 26 : 0
                visible: daySeparator.length > 0

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: dayLabel.left
                    anchors.rightMargin: 10
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                }
                Text {
                    id: dayLabel
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    text: daySeparator
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }

            Row {
                width: parent.width
                spacing: 10

                Item {
                    width: 40
                    height: grouped ? 1 : 40
                    Avatar {
                        anchors.top: parent.top
                        visible: !grouped
                        displayName: authorName
                    }
                }

                Column {
                    width: parent.width - 50
                    spacing: 2

                    Row {
                        visible: !grouped
                        spacing: 6
                        Text { text: authorName; color: Theme.textPrimary; font.bold: true }
                        Text { text: timeLabel; color: Theme.textSecondary; font.pixelSize: 11 }
                    }
                    Text {
                        visible: content.length > 0
                        text: content
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    MessageImage {
                        imageSource: imageUrl
                        maxWidth: 360
                    }
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
            width: parent.width - attachButton.width - sendButton.width - 16
            placeholderText: "Enviar mensagem..."
            onAccepted: sendButton.clicked()

            Keys.priority: Keys.BeforeItem
            Keys.onPressed: {
                if (event.matches(StandardKey.Paste)
                        && !chatController.uploading
                        && chatController.currentChannelId.length > 0) {
                    var path = clipboardController.pastedImagePath()
                    if (path.length > 0) {
                        chatController.sendImageMessage(messageField.text, path)
                        messageField.text = ""
                        event.accepted = true
                    }
                }
            }
        }

        AppButton {
            id: attachButton
            text: chatController.uploading ? "Enviando" : "Imagem"
            baseColor: Theme.surfaceAlt
            textColor: Theme.textPrimary
            enabled: !chatController.uploading && chatController.currentChannelId.length > 0
            onClicked: imagePicker.open()
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

    ImagePickerDialog {
        id: imagePicker
        onImageSelected: {
            chatController.sendImageMessage(messageField.text, selectedUrl.toString())
            messageField.text = ""
        }
    }
}
