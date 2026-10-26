import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"

// Modal color picker used by the server role editor. The hue is kept apart
// from `selectedColor` so picking a gray/black value does not lose the chosen
// hue while the user keeps dragging.
ComponentPopup {
    id: root
    title: "Escolher cor"
    width: 460
    standardButtons: Dialog.Ok | Dialog.Cancel

    property string initialColor: "#5865F2"
    property string selectedColor: "#5865F2"
    property bool rgbMode: false
    property real hue: 0
    property real saturation: 1
    property real value: 1

    function clamp(number, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, number))
    }

    function colorToHex(color) {
        function component(value) {
            var number = Math.round(clamp(value, 0, 1) * 255)
            var text = number.toString(16)
            return text.length === 1 ? "0" + text : text
        }
        return "#" + component(color.r) + component(color.g) + component(color.b)
    }

    function hexToColor(text) {
        var normalized = text.trim()
        if (normalized.length > 0 && normalized.charAt(0) !== "#")
            normalized = "#" + normalized
        if (!/^#[0-9a-fA-F]{6}$/.test(normalized))
            return null
        return Qt.rgba(parseInt(normalized.substr(1, 2), 16) / 255,
                       parseInt(normalized.substr(3, 2), 16) / 255,
                       parseInt(normalized.substr(5, 2), 16) / 255, 1)
    }

    function syncInputs() {
        if (!hexField.activeFocus)
            hexField.text = selectedColor
        var color = hexToColor(selectedColor)
        if (!color)
            return
        if (!redField.activeFocus)
            redField.text = Math.round(color.r * 255)
        if (!greenField.activeFocus)
            greenField.text = Math.round(color.g * 255)
        if (!blueField.activeFocus)
            blueField.text = Math.round(color.b * 255)
    }

    function applyHsv() {
        selectedColor = colorToHex(Qt.hsva(hue / 360, saturation, value, 1.0))
        syncInputs()
    }

    function setFromColor(color) {
        if (!color || isNaN(color.r) || color.r < 0)
            return
        if (color.hsvHue >= 0)
            hue = color.hsvHue
        saturation = color.hsvSaturation
        value = color.hsvValue
        applyHsv()
    }

    function applyHex(text) {
        var color = hexToColor(text)
        if (!color)
            return false
        setFromColor(color)
        return true
    }

    function applyRgb() {
        var red = clamp(parseInt(redField.text, 10) || 0, 0, 255)
        var green = clamp(parseInt(greenField.text, 10) || 0, 0, 255)
        var blue = clamp(parseInt(blueField.text, 10) || 0, 0, 255)
        setFromColor(Qt.rgba(red / 255, green / 255, blue / 255, 1))
    }

    function copyColor() {
        clipboardHelper.text = selectedColor
        clipboardHelper.selectAll()
        clipboardHelper.copy()
    }

    onOpened: {
        var color = hexToColor(initialColor)
        if (color)
            setFromColor(color)
    }

    contentItem: Column {
        width: 420
        spacing: 12

        Text {
            width: parent.width
            text: "Escolha uma cor, compare amostras e copie o valor exato."
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        Rectangle {
            id: svArea
            width: parent.width
            height: 150
            radius: Theme.radiusSmall
            clip: true
            color: Qt.hsva(root.hue / 360, 1, 1, 1)

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "white" }
                    GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
                }
            }

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: "black" }
                }
            }

            Rectangle {
                width: 16
                height: 16
                radius: 8
                x: root.saturation * svArea.width - width / 2
                y: (1 - root.value) * svArea.height - height / 2
                color: "transparent"
                border.width: 2
                border.color: "white"

                Rectangle {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    radius: 5
                    color: "transparent"
                    border.width: 1
                    border.color: "black"
                }
            }

            MouseArea {
                anchors.fill: parent

                function update(event) {
                    root.saturation = root.clamp(event.x / svArea.width, 0, 1)
                    root.value = 1 - root.clamp(event.y / svArea.height, 0, 1)
                    root.applyHsv()
                }

                onPressed: update(mouse)
                onPositionChanged: if (pressed) update(mouse)
            }
        }

        Item {
            width: parent.width
            height: 20

            Rectangle {
                id: hueBar
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 12
                radius: 6
                color: Theme.surfaceAlt

                Rectangle {
                    width: root.clamp(root.hue / 360, 0, 1) * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: Theme.accent
                }
            }

            Rectangle {
                width: 18
                height: 18
                radius: 9
                anchors.verticalCenter: parent.verticalCenter
                x: root.clamp((root.hue / 360) * hueBar.width - width / 2, 0, hueBar.width - width)
                color: Theme.surfaceAlt
                border.width: 2
                border.color: Theme.accent

                Rectangle {
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.accent
                }
            }

            MouseArea {
                anchors.fill: parent

                function update(event) {
                    root.hue = root.clamp(event.x / width, 0, 1) * 360
                    root.applyHsv()
                }

                onPressed: update(mouse)
                onPositionChanged: if (pressed) update(mouse)
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Rectangle {
                width: 40
                height: 40
                radius: 20
                color: root.selectedColor
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.2)
            }

            AppTextField {
                id: hexField
                width: 150
                visible: !root.rgbMode
                text: root.selectedColor
                onEditingFinished: {
                    if (!root.applyHex(text))
                        text = root.selectedColor
                }
            }

            Row {
                visible: root.rgbMode
                spacing: 4

                AppTextField {
                    id: redField
                    width: 50
                    onEditingFinished: root.applyRgb()
                }
                AppTextField {
                    id: greenField
                    width: 50
                    onEditingFinished: root.applyRgb()
                }
                AppTextField {
                    id: blueField
                    width: 50
                    onEditingFinished: root.applyRgb()
                }
            }

            AppButton {
                text: "Copiar"
                height: 40
                implicitHeight: 40
                baseColor: Theme.surfaceAlt
                textColor: Theme.textPrimary
                onClicked: root.copyColor()
            }

            AppButton {
                text: root.rgbMode ? "RGB" : "HEX"
                height: 40
                implicitHeight: 40
                baseColor: Theme.surfaceAlt
                textColor: Theme.textPrimary
                onClicked: root.rgbMode = !root.rgbMode
            }
        }

        Row {
            spacing: 8

            Repeater {
                model: ["#5865F2", "#1FD1C1", "#23A55A", "#F0B232", "#EB459E",
                        "#DA373C", "#8B5CF6", "#111214"]

                delegate: Rectangle {
                    width: 26
                    height: 26
                    radius: 13
                    color: modelData
                    border.width: root.selectedColor === modelData ? 2 : 0
                    border.color: Theme.textPrimary

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var color = root.hexToColor(modelData)
                            if (color)
                                root.setFromColor(color)
                        }
                    }
                }
            }
        }

        TextEdit {
            id: clipboardHelper
            visible: false
            width: 1
            height: 1
        }
    }
}
