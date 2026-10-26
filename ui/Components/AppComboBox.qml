import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

ComboBox {
    id: control
    implicitWidth: 180
    implicitHeight: 40

    palette.window: Theme.surface
    palette.windowText: Theme.textPrimary
    palette.base: Theme.surfaceAlt
    palette.text: Theme.textPrimary
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: "white"

    contentItem: Text {
        leftPadding: 12
        rightPadding: control.indicator.width + 12
        text: control.displayText
        font: control.font
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 12
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "\u25be"
        color: control.visualFocus ? Theme.accent : Theme.textSecondary
        font.pixelSize: 12
        visible: !control.flat || control.down
    }

    background: Rectangle {
        implicitHeight: 40
        radius: Theme.radiusSmall
        color: Theme.surface
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Qt.rgba(0.5, 0.5, 0.5, 0.35)
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 280)
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.surface
            border.width: 1
            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.35)
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }

    delegate: ItemDelegate {
        id: item
        width: control.width - 8
        implicitHeight: 36
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        text: control.textRole
              ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
              : modelData

        contentItem: Text {
            text: item.text
            color: item.highlighted ? "white" : Theme.textPrimary
            font: item.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: item.highlighted ? Theme.accent
                                    : (item.hovered ? Theme.surfaceAlt : "transparent")
        }
    }
}
