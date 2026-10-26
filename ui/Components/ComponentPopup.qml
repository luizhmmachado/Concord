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

    palette.window: Theme.surface
    palette.windowText: Theme.textPrimary
    palette.base: Theme.surfaceAlt
    palette.alternateBase: Theme.surface
    palette.text: Theme.textPrimary
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: "white"
    palette.brightText: "white"
    palette.mid: Theme.surfaceAlt
    palette.dark: Theme.surfaceAlt
    palette.shadow: "black"

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusMedium
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.12)
    }

    header: Label {
        text: root.title
        visible: root.title.length > 0
        elide: Label.ElideRight
        font.pixelSize: 18
        font.bold: true
        padding: 16
        color: Theme.textPrimary

        background: Rectangle {
            color: "transparent"
        }
    }

    footer: DialogButtonBox {
        visible: count > 0
        padding: 8
        spacing: 8

        delegate: AppButton {
            baseColor: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                       ? Theme.accent : Theme.surfaceAlt
            textColor: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                       ? "white" : Theme.textPrimary
        }

        background: Rectangle {
            color: "transparent"
        }
    }
}
