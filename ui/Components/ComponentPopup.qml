import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

// Shared shell for every popup/dialog in the app.
//
// The visual parent is forced to the window overlay so a popup declared inside
// a narrow column (for example the member sidebar) is still centered on the
// whole window instead of on its declaration parent.
Dialog {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent

    modal: true
    focus: true
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.12)
    }
}
