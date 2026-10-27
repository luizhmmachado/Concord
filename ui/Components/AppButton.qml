import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

Button {
    id: root
    property color baseColor: Theme.accent
    property color textColor: "white"
    property string iconName: ""
    property int iconSize: 16
    implicitHeight: 40
    implicitWidth: contentItem.implicitWidth + 32

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.pressed ? Qt.darker(root.baseColor, 1.2) : (root.hovered ? Qt.lighter(root.baseColor, 1.1) : root.baseColor)
        opacity: root.enabled ? 1.0 : 0.5
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: root.iconName.length > 0 && root.text.length > 0 ? 8 : 0

            AppIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName.length > 0
                name: root.iconName
                size: root.iconSize
                color: root.textColor
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.text.length > 0
                text: root.text
                color: root.textColor
                font.pixelSize: 14
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
