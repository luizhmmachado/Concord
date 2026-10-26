import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Second column: the channel list of the currently selected server.
Rectangle {
    id: root
    width: 232
    color: Theme.surface

    signal channelSelected(string channelId, string type)

    property string selectedChannelId: ""

    function participantsIn(channelId) {
        return callPresence.presence[channelId] || []
    }

    Connections {
        target: serverController
        function onCurrentServerChanged() { callPresence.watchServer(serverController.currentServerId) }
    }

    Component.onCompleted: callPresence.watchServer(serverController.currentServerId)

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        Text {
            text: "CANAIS"
            color: Theme.textSecondary
            font.bold: true
            font.pixelSize: 12
        }

        AppButton {
            width: root.width - 16
            visible: serverController.currentServerId.length > 0
            text: "\u2795 Convidar pessoas"
            baseColor: Theme.accent
            onClicked: serverInvitePopup.open()
        }

        Repeater {
            model: serverController.channels
            delegate: Column {
                width: root.width - 16
                spacing: 4

                Rectangle {
                    width: root.width - 16
                    height: 32
                    radius: Theme.radiusSmall
                    color: root.selectedChannelId === channelId ? Theme.surfaceAlt : "transparent"

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        spacing: 6

                        Text { 
                            text: type === "voice" ? "🔊" : "#"
                            color: (type === "voice" && callController.inCall && callController.callChannelId === channelId) ? Theme.accent : Theme.textSecondary
                        }
                        Text { 
                            text: name
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: type === "voice" && root.participantsIn(channelId).length > 0
                            text: "(" + root.participantsIn(channelId).length + ")"
                            color: Theme.online
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.selectedChannelId = channelId
                            root.channelSelected(channelId, type)
                        }
                    }
                }

                // Vem do banco, então aparece mesmo para quem não entrou na chamada.
                Repeater {
                    model: type === "voice" ? root.participantsIn(channelId) : []
                    delegate: Rectangle {
                        width: root.width - 32
                        height: 28
                        radius: Theme.radiusSmall
                        color: Theme.surfaceAlt
                        anchors.left: parent.left
                        anchors.leftMargin: 16

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            spacing: 6

                            Text {
                                text: "\u25cf"
                                color: Theme.online
                                font.pixelSize: 14
                            }

                            Text {
                                text: modelData.displayName
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Row {
                                spacing: 3
                                Text {
                                    visible: modelData.muted
                                    text: "\ud83d\udd07"
                                    font.pixelSize: 9
                                }
                                Text {
                                    visible: modelData.cameraOn
                                    text: "\ud83d\udcf7"
                                    color: Theme.accent
                                    font.pixelSize: 9
                                }
                                Text {
                                    visible: modelData.screenSharing
                                    text: "\ud83d\udda5"
                                    color: Theme.accent
                                    font.pixelSize: 9
                                }
                            }
                        }
                    }
                }
            }
        }

        AppButton {
            text: "+ Canal"
            visible: serverController.currentServerId.length > 0 && serverMembers.canManageChannels
            baseColor: Theme.surfaceAlt
            onClicked: createChannelDialog.open()
        }
    }

    ServerInvitePopup { id: serverInvitePopup }

    ComponentPopup {
        id: createChannelDialog
        title: "Novo canal"
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 8
            AppTextField { id: channelNameField; placeholderText: "nome-do-canal"; width: 240 }
            Row {
                spacing: 8
                AppRadioButton { id: textTypeRadio; text: "Texto"; checked: true }
                AppRadioButton { id: voiceTypeRadio; text: "Voz" }
            }
        }

        onAccepted: {
            if (channelNameField.text.length === 0)
                return
            serverController.createChannel(serverController.currentServerId, channelNameField.text, voiceTypeRadio.checked ? "voice" : "text")
            channelNameField.text = ""
        }
    }
}
