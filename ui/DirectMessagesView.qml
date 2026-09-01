import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Private conversations: contact drawer on the left, thread on the right.
Rectangle {
    id: root
    anchors.fill: parent
    color: Theme.background

    Component.onCompleted: directMessageController.loadConversations()

    Rectangle {
        id: drawer
        width: 260
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: Theme.surface

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text {
                text: "MENSAGENS DIRETAS"
                color: Theme.textSecondary
                font.bold: true
                font.pixelSize: 12
            }

            AppButton {
                Layout.fillWidth: true
                text: "+ Adicionar amigo"
                baseColor: Theme.surfaceAlt
                onClicked: addFriendDialog.open()
            }

            ListView {
                id: conversationList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: directMessageController.conversations
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    width: conversationList.width
                    height: 56
                    radius: Theme.radiusSmall
                    color: directMessageController.currentConversationId === conversationId
                           ? Theme.surfaceAlt
                           : (hover.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")

                    HoverHandler { id: hover }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10

                        Avatar {
                            anchors.verticalCenter: parent.verticalCenter
                            displayName: peerName
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 58
                            spacing: 2

                            Text {
                                width: parent.width
                                text: peerName
                                color: Theme.textPrimary
                                font.bold: true
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: lastMessage.length > 0 ? lastMessage : "Nenhuma mensagem ainda"
                                color: Theme.textSecondary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: directMessageController.openConversation(conversationId)
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.left: drawer.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.background

        Rectangle {
            id: threadHeader
            visible: directMessageController.currentConversationId.length > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? 48 : 0
            color: Theme.surface

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                spacing: 10

                Avatar {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    displayName: directMessageController.currentPeerName
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: directMessageController.currentPeerName
                    color: Theme.textPrimary
                    font.bold: true
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: directMessageController.currentConversationId.length === 0
            text: "Selecione uma conversa"
            color: Theme.textSecondary
            font.pixelSize: 16
        }

        ListView {
            id: messageList
            visible: directMessageController.currentConversationId.length > 0
            anchors.top: threadHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: inputRow.top
            anchors.margins: 12
            clip: true
            spacing: 4
            model: directMessageController.messages
            ScrollBar.vertical: ScrollBar {}

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
                            text: content
                            color: Theme.textPrimary
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }
                }
            }

            onCountChanged: positionViewAtEnd()
        }

        Row {
            id: inputRow
            visible: directMessageController.currentConversationId.length > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 8

            AppTextField {
                id: messageField
                width: parent.width - sendButton.width - 8
                placeholderText: "Enviar mensagem para " + directMessageController.currentPeerName + "..."
                onAccepted: sendButton.clicked()
            }

            AppButton {
                id: sendButton
                text: "Enviar"
                onClicked: {
                    directMessageController.sendMessage(messageField.text)
                    messageField.text = ""
                }
            }
        }
    }

    Dialog {
        id: addFriendDialog
        title: "Adicionar amigo"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 8
            Text {
                text: "Cole o link de convite ou digite o nome de usuário:"
                color: Theme.textPrimary
                font.pixelSize: 12
            }
            AppTextField {
                id: friendField
                placeholderText: "usuario ou concord://user/usuario"
                width: 300
            }
        }

        onAccepted: {
            directMessageController.addFriend(friendField.text)
            friendField.text = ""
        }
    }
}
