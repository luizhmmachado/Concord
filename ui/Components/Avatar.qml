import QtQuick 2.15
import "qrc:/style"

// Circular avatar with initials fallback (no network image caching yet).
Rectangle {
    id: root
    property string displayName: "?"
    property url avatarUrl: ""
    property bool showStatusDot: false
    property bool online: false

    width: 40
    height: 40
    radius: width / 2
    color: Theme.accent

    Text {
        anchors.centerIn: parent
        visible: avatarUrl.toString().length === 0
        text: root.displayName.length > 0 ? root.displayName.charAt(0).toUpperCase() : "?"
        color: "white"
        font.bold: true
        font.pixelSize: 16
    }

    Image {
        anchors.fill: parent
        visible: avatarUrl.toString().length > 0
        source: avatarUrl
        fillMode: Image.PreserveAspectCrop
    }

    Rectangle {
        visible: root.showStatusDot
        width: 12
        height: 12
        radius: 6
        color: root.online ? Theme.online : Theme.textSecondary
        border.width: 2
        border.color: Theme.background
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
