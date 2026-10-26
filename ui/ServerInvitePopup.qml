import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Discord-style invite sheet: pick a friend to DM the invite to, or copy the link.
ComponentPopup {
    id: root
    padding: 0
    width: 460
    height: 540

    readonly property bool hasInviteCode: serverController.currentServerInviteCode.length > 0
    readonly property string inviteLink: hasInviteCode
                                          ? "concord://invite/" + serverController.currentServerInviteCode
                                          : ""

    property string sentToConversationId: ""

    onOpened: {
        sentToConversationId = ""
        directMessageController.loadConversations()
        serverController.requestInviteCode()
    }

    Connections {
        target: serverController
        function onCurrentServerChanged() {
            if (!root.opened)
                return
            root.sentToConversationId = ""
            serverController.requestInviteCode()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: "Convidar amigos para " + serverController.currentServerName
                color: Theme.textPrimary
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                text: "\u2715"
                color: Theme.textSecondary
                font.pixelSize: 18
                MouseArea { anchors.fill: parent; onClicked: root.close() }
            }
        }

        Text {
            visible: serverController.inviteCodeLoading
            Layout.fillWidth: true
            text: "Gerando convite..."
            color: Theme.textSecondary
            font.pixelSize: 11
        }

        Text {
            visible: !serverController.inviteCodeLoading && !root.hasInviteCode
            Layout.fillWidth: true
            text: "Não foi possível gerar o convite. Verifique se você ainda pode convidar pessoas."
            color: Theme.danger
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        AppTextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Buscar amigos"
        }

        ListView {
            id: friendList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: directMessageController.conversations
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                width: friendList.width
                height: matches ? 56 : 0
                visible: matches
                radius: Theme.radiusSmall
                color: hover.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent"

                readonly property bool matches: searchField.text.length === 0
                                                || peerName.toLowerCase().indexOf(searchField.text.toLowerCase()) >= 0
                                                || peerUsername.toLowerCase().indexOf(searchField.text.toLowerCase()) >= 0

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
                        width: parent.width - 160
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
                            text: peerUsername
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    AppButton {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.sentToConversationId === conversationId ? "Enviado" : "Convidar"
                        enabled: root.sentToConversationId !== conversationId && root.hasInviteCode
                        baseColor: root.sentToConversationId === conversationId ? Theme.surfaceAlt : Theme.accent
                        onClicked: {
                            directMessageController.sendServerInvite(conversationId,
                                                                    serverController.currentServerInviteCode,
                                                                    serverController.currentServerName)
                            root.sentToConversationId = conversationId
                        }
                    }
                }
            }
        }

        Text {
            visible: directMessageController.conversations.count === 0
            Layout.fillWidth: true
            text: "Você ainda não tem conversas. Adicione um amigo em Mensagens Diretas para convidá-lo por aqui."
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

        Text {
            text: "Ou envie um convite do servidor a um amigo"
            color: Theme.textPrimary
            font.bold: true
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AppTextField {
                id: linkField
                Layout.fillWidth: true
                readOnly: true
                text: root.inviteLink
            }

            AppButton {
                text: "Copiar"
                enabled: root.hasInviteCode
                onClicked: {
                    linkField.selectAll()
                    linkField.copy()
                    linkField.deselect()
                }
            }
        }
    }
}
