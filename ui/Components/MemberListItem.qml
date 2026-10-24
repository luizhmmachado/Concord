import QtQuick 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// One row in the server member sidebar.
Rectangle {
    id: root

    property string userId: ""
    property string displayName: ""
    property string username: ""
    property string avatarUrl: ""
    property bool online: false
    property string roleName: ""
    property string roleColor: ""

    signal clicked()

    width: parent ? parent.width : 0
    height: 42
    radius: Theme.radiusSmall
    color: hoverArea.containsMouse ? Theme.surfaceAlt : "transparent"

    Row {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 8

        Avatar {
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            displayName: root.displayName
            avatarUrl: root.avatarUrl
            showStatusDot: true
            online: root.online
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: root.width - 58
            spacing: 1

            Text {
                width: parent.width
                text: root.displayName.length > 0 ? root.displayName : root.username
                color: Theme.textPrimary
                font.pixelSize: 13
                elide: Text.ElideRight
            }

            Text {
                visible: root.roleName.length > 0
                width: parent.width
                text: root.roleName
                color: root.roleColor.length > 0 ? root.roleColor : Theme.textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: root.clicked()
    }
}
