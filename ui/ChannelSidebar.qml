import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// Second column: the channel list of the currently selected server.
Rectangle {
    id: root
    width: 232
    color: Theme.surface

    signal channelSelected(string channelId, string type)

    property string selectedChannelId: ""

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

        Repeater {
            model: serverController.channels
            delegate: Rectangle {
                width: root.width - 16
                height: 32
                radius: Theme.radiusSmall
                color: root.selectedChannelId === channelId ? Theme.surfaceAlt : "transparent"

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    spacing: 6
                    Text { text: type === "voice" ? "🔊" : "#"; color: Theme.textSecondary }
                    Text { text: name; color: Theme.textPrimary }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        root.selectedChannelId = channelId
                        root.channelSelected(channelId, type)
                    }
                }
            }
        }

        AppButton {
            text: "+ Canal"
            baseColor: Theme.surfaceAlt
            onClicked: createChannelDialog.open()
        }
    }

    Dialog {
        id: createChannelDialog
        title: "Novo canal"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 8
            AppTextField { id: channelNameField; placeholderText: "nome-do-canal"; width: 240 }
            Row {
                spacing: 8
                RadioButton { id: textTypeRadio; text: "Texto"; checked: true }
                RadioButton { id: voiceTypeRadio; text: "Voz" }
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
