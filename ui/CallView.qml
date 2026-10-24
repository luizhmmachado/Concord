import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia 5.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    id: root
    color: Theme.surfaceAlt
    anchors.fill: parent

    // Transmissões remotas ficam esmaecidas até o usuário optar por assistir.
    property var watching: ({})

    function isWatching(userId) {
        return watching[userId] === true
    }

    function toggleWatching(userId) {
        var next = watching
        next[userId] = !next[userId]
        watching = next
    }

    Item {
        id: shareStage
        visible: callController.anyoneScreenSharing
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: visible ? 12 : 0
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        height: visible ? root.height * 0.42 : 0

        ListView {
            id: shareList
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: 10
            clip: true
            model: callController.participants
            ScrollBar.horizontal: ScrollBar {}

            delegate: Item {
                id: shareTile
                required property string userId
                required property string displayName
                required property bool screenSharing
                required property var screenSource

                readonly property bool isLocal: shareTile.userId === authController.userId
                // O próprio preview nunca é ocultado.
                readonly property bool revealed: isLocal || root.isWatching(shareTile.userId)
                readonly property real aspect: {
                    var size = shareTile.screenSource ? shareTile.screenSource.frameSize : null
                    return (size && size.height > 0) ? size.width / size.height : 16 / 9
                }

                visible: shareTile.screenSharing
                width: visible ? Math.min(shareList.width, shareList.height * aspect) : 0
                height: shareList.height

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusMedium
                    color: Theme.surface
                    border.width: 1
                    border.color: shareTile.revealed ? Qt.rgba(0.12, 0.82, 0.76, 0.55)
                                                     : Qt.rgba(1, 1, 1, 0.12)
                    clip: true

                    VideoOutput {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: shareTile.screenSource
                        fillMode: VideoOutput.PreserveAspectFit
                        opacity: shareTile.revealed ? 1.0 : 0.15
                    }

                    Column {
                        anchors.centerIn: parent
                        visible: !shareTile.revealed
                        spacing: 10

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: shareTile.displayName + " está transmitindo"
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            font.bold: true
                        }
                        AppButton {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Assistir"
                            onClicked: root.toggleWatching(shareTile.userId)
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.margins: 8
                        width: label.implicitWidth + 16
                        height: 24
                        radius: 12
                        color: Qt.rgba(0, 0, 0, 0.55)

                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: shareTile.isLocal ? "Sua transmissão" : shareTile.displayName
                            color: "white"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    AppButton {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 8
                        visible: shareTile.revealed && !shareTile.isLocal
                        text: "Parar de assistir"
                        baseColor: Theme.surfaceAlt
                        onClicked: root.toggleWatching(shareTile.userId)
                    }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: callController.screenSharing && !screenCaptureSupported
            text: "Compartilhamento de tela indisponível neste ambiente (Wayland)."
            color: Theme.textPrimary
            font.pixelSize: 20
            font.bold: true
        }
    }

    GridView {
        id: grid
        anchors.top: shareStage.visible ? shareStage.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controlBar.top
        anchors.margins: 12

        readonly property int tileCount: Math.max(1, count)
        readonly property int columns: Math.max(1, Math.ceil(Math.sqrt(tileCount)))
        readonly property int rows: Math.max(1, Math.ceil(tileCount / columns))

        cellWidth: Math.floor(width / columns)
        cellHeight: Math.floor(height / rows)
        model: callController.participants
        interactive: false
        clip: true

        delegate: Item {
            id: participantCell
            required property string userId
            required property string displayName
            required property bool muted
            required property bool cameraOn
            required property bool screenSharing
            required property string connectionState
            required property var videoSource

            readonly property bool isLocal: participantCell.userId === authController.userId
            readonly property bool showCamera: participantCell.videoSource !== null
                                               && (isLocal ? callController.cameraEnabled : participantCell.cameraOn)

            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: Theme.radiusMedium
                color: Theme.surface
                clip: true

                Avatar {
                    anchors.centerIn: parent
                    visible: !participantCell.showCamera
                    displayName: participantCell.displayName
                    width: Math.min(parent.width, parent.height) * 0.4
                    height: width
                }

                VideoOutput {
                    anchors.fill: parent
                    visible: participantCell.showCamera
                    source: participantCell.videoSource
                    fillMode: VideoOutput.PreserveAspectCrop
                }

                Text {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: statusRow.left
                    anchors.margins: 8
                    text: participantCell.displayName
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                Row {
                    id: statusRow
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 8
                    spacing: 4
                    Text { visible: participantCell.muted; text: "🔇"; font.pixelSize: 11 }
                    Text { visible: participantCell.screenSharing; text: "🖥"; color: Theme.accent; font.pixelSize: 11 }
                }

                Text {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    text: participantCell.connectionState
                    color: Theme.textSecondary
                    font.pixelSize: 9
                }
            }
        }
    }

    Row {
        id: controlBar
        visible: callController.inCall
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 16
        spacing: 12

        AppButton {
            text: callController.micMuted ? "🔇 Desmutar" : "🎙 Mutar"
            baseColor: callController.micMuted ? Theme.danger : Theme.surface
            onClicked: callController.toggleMicMuted()
        }
        AppButton {
            text: callController.cameraEnabled ? "📷 Desligar câmera" : "📷 Ligar câmera"
            baseColor: callController.cameraEnabled ? Theme.accent : Theme.surface
            onClicked: callController.toggleCamera()
        }
        AppButton {
            text: callController.screenSharing ? "🖥 Parar transmissão" : "🖥 Transmitir tela"
            baseColor: callController.screenSharing ? Theme.accent : Theme.surface
            onClicked: {
                if (callController.screenSharing) {
                    callController.stopScreenShare()
                } else {
                    callController.screenShareSources.refresh()
                    sourcePicker.open()
                }
            }
        }
        AppButton {
            text: "📞 Sair"
            baseColor: Theme.danger
            onClicked: callController.leaveCall()
        }
    }

    ComponentPopup {
        id: sourcePicker
        width: 520
        height: 560
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property int selectedIndex: -1
        property int quality: 1080
        property int fps: 30

        function startShare(index) {
            callController.screenShareHeight = quality
            callController.screenShareFps = fps
            callController.startScreenShare(index)
            close()
        }

        onOpened: {
            selectedIndex = -1
            quality = callController.screenShareHeight
            fps = callController.screenShareFps
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Text {
                text: "O que você quer transmitir?"
                color: Theme.textPrimary
                font.pixelSize: 18
                font.bold: true
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: callController.screenShareSources
                spacing: 4
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 46
                    radius: Theme.radiusSmall
                    color: sourcePicker.selectedIndex === index
                           ? Theme.accent
                           : (hover.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent")

                    HoverHandler { id: hover }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: isWindow ? "🪟" : "🖥"
                            font.pixelSize: 18
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 40
                            text: title
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }

                    TapHandler {
                        onTapped: sourcePicker.selectedIndex = index
                        onDoubleTapped: sourcePicker.startShare(index)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Qualidade"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    Layout.preferredWidth: 76
                }
                AppButton {
                    text: "720p"
                    baseColor: sourcePicker.quality === 720 ? Theme.accent : Theme.surfaceAlt
                    textColor: sourcePicker.quality === 720 ? "white" : Theme.textPrimary
                    onClicked: sourcePicker.quality = 720
                }
                AppButton {
                    text: "1080p"
                    baseColor: sourcePicker.quality === 1080 ? Theme.accent : Theme.surfaceAlt
                    textColor: sourcePicker.quality === 1080 ? "white" : Theme.textPrimary
                    onClicked: sourcePicker.quality = 1080
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Quadros/s"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    Layout.preferredWidth: 76
                }
                AppButton {
                    text: "30 fps"
                    baseColor: sourcePicker.fps === 30 ? Theme.accent : Theme.surfaceAlt
                    textColor: sourcePicker.fps === 30 ? "white" : Theme.textPrimary
                    onClicked: sourcePicker.fps = 30
                }
                AppButton {
                    text: "60 fps"
                    baseColor: sourcePicker.fps === 60 ? Theme.accent : Theme.surfaceAlt
                    textColor: sourcePicker.fps === 60 ? "white" : Theme.textPrimary
                    onClicked: sourcePicker.fps = 60
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                AppButton {
                    text: "Cancelar"
                    baseColor: Theme.surfaceAlt
                    onClicked: sourcePicker.close()
                }
                AppButton {
                    text: "Transmitir"
                    baseColor: Theme.accent
                    enabled: sourcePicker.selectedIndex >= 0
                    opacity: enabled ? 1 : 0.5
                    onClicked: sourcePicker.startShare(sourcePicker.selectedIndex)
                }
            }
        }
    }
}
