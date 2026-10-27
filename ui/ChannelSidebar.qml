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
                     && serverMembers.currentServerId === serverController.currentServerId
                     && serverMembers.canInvite
            text: "Convidar pessoas"
            iconName: "user-plus"
            iconSize: 16
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

                        AppIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: type === "voice" ? "channel-voice" : "channel-text"
                            size: 14
                            color: (type === "voice" && callController.inCall && callController.callChannelId === channelId) ? Theme.accent : Theme.textSecondary
                        }
                        Text { 
                            anchors.verticalCenter: parent.verticalCenter
                            text: name
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
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

                            AppIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "status-online"
                                size: 14
                                color: Theme.online
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.displayName
                                color: Theme.textPrimary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 3
                                AppIcon {
                                    visible: modelData.muted
                                    name: "mic-off-16"
                                    size: 9
                                    color: Theme.textSecondary
                                }
                                AppIcon {
                                    visible: modelData.cameraOn
                                    name: "camera-on-16"
                                    size: 9
                                    color: Theme.accent
                                }
                                AppIcon {
                                    visible: modelData.screenSharing
                                    name: "screen-share-on-16"
                                    size: 9
                                    color: Theme.accent
                                }
                            }
                        }
                    }
                }
            }
        }

        AppButton {
            text: "Canal"
            iconName: "plus"
            iconSize: 16
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
