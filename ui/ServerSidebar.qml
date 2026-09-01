import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// Left-most rail: server icons + a settings shortcut at the bottom.
Rectangle {
    id: root
    width: 72
    color: Theme.surfaceAlt

    signal serverSelected(string serverId)
    signal createServerRequested()
    signal settingsRequested()

    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 12
        spacing: 12

        Repeater {
            model: serverController.servers
            delegate: Rectangle {
                width: 48
                height: 48
                radius: 16
                color: serverController.currentServerId === serverId ? Theme.accent : Theme.surface

                Text {
                    anchors.centerIn: parent
                    text: name.length > 0 ? name.charAt(0).toUpperCase() : "?"
                    color: "white"
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        serverController.selectServer(serverId)
                        root.serverSelected(serverId)
                    }
                }
            }
        }

        Rectangle {
            width: 48
            height: 48
            radius: 16
            color: Theme.surface
            Text { anchors.centerIn: parent; text: "+"; color: "white"; font.pixelSize: 22 }
            MouseArea { anchors.fill: parent; onClicked: root.createServerRequested() }
        }
    }

    Rectangle {
        width: 48
        height: 48
        radius: 16
        color: Theme.surface
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 12
        Text { anchors.centerIn: parent; text: "⚙"; color: "white"; font.pixelSize: 20 }
        MouseArea { anchors.fill: parent; onClicked: root.settingsRequested() }
    }
}
