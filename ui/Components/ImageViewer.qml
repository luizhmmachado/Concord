import QtQuick 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// Full-window image viewer with pan, zoom, download and Escape-to-close.
Rectangle {
    id: root
    property string imageSource: ""
    property real zoom: 1.0
    property real offsetX: 0
    property real offsetY: 0
    property bool downloading: false
    property string statusMessage: ""

    signal closeRequested()

    visible: false
    focus: visible
    color: Qt.rgba(0, 0, 0, 0.94)

    readonly property real fitScale: {
        if (image.implicitWidth <= 0 || image.implicitHeight <= 0)
            return 1;
        if (viewport.width <= 0 || viewport.height <= 0)
            return 1;
        return Math.min(1, Math.min(viewport.width / image.implicitWidth,
                                     viewport.height / image.implicitHeight));
    }

    onVisibleChanged: {
        if (visible) {
            forceActiveFocus()
            resetView()
            downloading = false
            statusMessage = ""
            Qt.callLater(function() { statusTimer.stop() })
        } else {
            downloading = false
            statusMessage = ""
        }
    }
    onImageSourceChanged: resetView()

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, value));
    }
    function imageWidth() {
        return Math.max(1, image.implicitWidth * fitScale * zoom);
    }
    function imageHeight() {
        return Math.max(1, image.implicitHeight * fitScale * zoom);
    }
    function maxOffsetX() {
        return Math.max(0, (imageWidth() - viewport.width) / 2);
    }
    function maxOffsetY() {
        return Math.max(0, (imageHeight() - viewport.height) / 2);
    }
    function clampX(value) {
        return clamp(value, -maxOffsetX(), maxOffsetX());
    }
    function clampY(value) {
        return clamp(value, -maxOffsetY(), maxOffsetY());
    }
    function zoomBy(factor) {
        var next = clamp(zoom * factor, 0.25, 5.0);
        if (Math.abs(next - zoom) < 0.0001)
            return;
        zoom = next;
        offsetX = clampX(offsetX);
        offsetY = clampY(offsetY);
    }
    function resetView() {
        zoom = 1.0;
        offsetX = 0;
        offsetY = 0;
    }
    function handleWheel(wheel) {
        wheel.accepted = true;
        if (wheel.angleDelta.y > 0)
            zoomBy(1.15);
        else if (wheel.angleDelta.y < 0)
            zoomBy(1 / 1.15);
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.closeRequested()
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            zoomBy(1.2);
            event.accepted = true;
        } else if (event.key === Qt.Key_Minus) {
            zoomBy(1 / 1.2);
            event.accepted = true;
        } else if (event.key === Qt.Key_0) {
            resetView();
            event.accepted = true;
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.forceActiveFocus()
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: Qt.rgba(0.04, 0.04, 0.05, 0.92)

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
        }

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: "Imagem"
            color: Theme.textPrimary
            font.pixelSize: 16
            font.bold: true
        }
        Text {
            anchors.left: titleText.right
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: Math.round(root.zoom * 100) + "%"
            color: Theme.textSecondary
            font.pixelSize: 13
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            AppButton {
                text: "Baixar"
                enabled: root.imageSource.length > 0 && !root.downloading
                onClicked: {
                    root.downloading = true;
                    root.statusMessage = "Baixando imagem...";
                    statusTimer.stop();
                    imageController.downloadImage(root.imageSource);
                }
            }
            AppButton {
                text: "Ajustar"
                baseColor: Theme.surfaceAlt
                textColor: Theme.textPrimary
                onClicked: root.resetView()
            }
            AppButton {
                text: ""
                iconName: "minus"
                iconSize: 14
                baseColor: Theme.surfaceAlt
                textColor: Theme.textPrimary
                enabled: root.zoom > 0.2501
                onClicked: root.zoomBy(1 / 1.15)
            }
            AppButton {
                text: ""
                iconName: "plus"
                iconSize: 14
                baseColor: Theme.surfaceAlt
                textColor: Theme.textPrimary
                enabled: root.zoom < 4.999
                onClicked: root.zoomBy(1.15)
            }
            AppButton {
                text: "Fechar"
                baseColor: Theme.danger
                onClicked: root.closeRequested()
            }
        }
    }

    Item {
        id: viewport
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
            onWheel: root.handleWheel(wheel)
        }

        Image {
            id: image
            source: root.imageSource
            width: Math.max(1, implicitWidth * root.fitScale * root.zoom)
            height: Math.max(1, implicitHeight * root.fitScale * root.zoom)
            x: (viewport.width - width) / 2 + root.offsetX
            y: (viewport.height - height) / 2 + root.offsetY
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: true
            smooth: true
            mipmap: true
            opacity: status === Image.Ready ? 1 : 0

            property point lastScenePoint: Qt.point(0, 0)

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                cursorShape: pressed ? Qt.ClosedHandCursor
                                     : (root.zoom > 1.001 ? Qt.OpenHandCursor : Qt.ArrowCursor)
                onPressed: {
                    root.forceActiveFocus();
                    image.lastScenePoint = mapToItem(null, mouse.x, mouse.y);
                }
                onPositionChanged: {
                    if (!pressed)
                        return;
                    var current = mapToItem(null, mouse.x, mouse.y);
                    root.offsetX = root.clampX(root.offsetX + current.x - image.lastScenePoint.x);
                    root.offsetY = root.clampY(root.offsetY + current.y - image.lastScenePoint.y);
                    image.lastScenePoint = current;
                }
                onWheel: root.handleWheel(wheel)
                onDoubleClicked: {
                    if (root.zoom > 1.001)
                        root.resetView();
                    else
                        root.zoomBy(2.0);
                }
            }
        }
    }

    Rectangle {
        visible: root.statusMessage.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: statusText.width + 24
        height: statusText.implicitHeight + 16
        radius: Theme.radiusSmall
        color: Qt.rgba(0.05, 0.05, 0.06, 0.94)
        border.color: Qt.rgba(1, 1, 1, 0.12)

        MouseArea {
            anchors.fill: parent
            onClicked: root.forceActiveFocus()
        }

        Text {
            id: statusText
            anchors.centerIn: parent
            width: Math.max(120, Math.min(720, root.width - 80))
            text: root.statusMessage
            color: Theme.textPrimary
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }
    }

    Timer {
        id: statusTimer
        interval: 5000
        onTriggered: root.statusMessage = ""
    }

    Connections {
        target: imageController
        function onDownloadFinished(success, path, error) {
            root.downloading = false;
            if (success)
                root.statusMessage = "Imagem salva em " + path;
            else
                root.statusMessage = "Falha ao baixar: " + (error.length > 0 ? error : "erro desconhecido");
            statusTimer.restart();
        }
    }
}
