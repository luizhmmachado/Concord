import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

Button {
    id: root
    property color baseColor: Theme.accent
    property color textColor: "white"
    implicitHeight: 40
    implicitWidth: contentItem.implicitWidth + 32

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.pressed ? Qt.darker(root.baseColor, 1.2) : (root.hovered ? Qt.lighter(root.baseColor, 1.1) : root.baseColor)
        opacity: root.enabled ? 1.0 : 0.5
    }

    contentItem: Text {
        text: root.text
        color: root.textColor
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
