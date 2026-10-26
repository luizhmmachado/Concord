import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

CheckBox {
    id: control
    spacing: 10
    topPadding: 4
    bottomPadding: 4
    implicitHeight: Math.max(indicator.implicitHeight, contentItem.implicitHeight) + topPadding + bottomPadding

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 4
        opacity: control.enabled ? 1 : 0.5
        color: control.checked ? Theme.accent : Theme.surface
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus || control.checked
                      ? Theme.accent
                      : (control.hovered ? Theme.textSecondary : Qt.rgba(0.5, 0.5, 0.5, 0.45))

        Text {
            anchors.centerIn: parent
            text: "\u2713"
            color: "white"
            font.pixelSize: 12
            font.bold: true
            visible: control.checked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textSecondary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
