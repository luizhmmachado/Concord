import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

TextField {
    id: root
    color: Theme.textPrimary
    placeholderTextColor: Theme.textSecondary
    selectByMouse: true
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.surfaceAlt
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.accent
    }
}
