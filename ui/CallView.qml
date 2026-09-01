import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Video call surface: a grid of participant tiles + the control bar
// (mute mic, toggle camera, toggle screen-share, hang up).
//
// NOTE: remote video/screen frames from GStreamer are not yet bridged into
// a QML-paintable surface (would need a QQuickFramebufferObject/appsink
// bridge); today this view only reflects call state (who is connected,
// muted, camera/screen status), not actual pixels. See README.
Rectangle {
    color: Theme.surfaceAlt
    anchors.fill: parent

    GridView {
        id: grid
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controlBar.top
        anchors.margins: 12
        cellWidth: width / Math.max(1, Math.ceil(Math.sqrt(count)))
        cellHeight: cellWidth * 0.75
        model: callController.participants

        delegate: Rectangle {
            width: grid.cellWidth - 8
            height: grid.cellHeight - 8
            radius: Theme.radiusMedium
            color: Theme.surface

            Avatar { anchors.centerIn: parent; displayName: displayName; width: 64; height: 64 }

            Row {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.margins: 6
                spacing: 4
                Text { text: displayName; color: Theme.textPrimary; font.bold: true }
                Text { visible: muted; text: "🔇"; color: Theme.danger }
                Text { visible: screenSharing; text: "🖥"; color: Theme.online }
            }

            Text {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 6
                text: connectionState
                color: Theme.textSecondary
                font.pixelSize: 10
            }
        }
    }

    Row {
        id: controlBar
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
            onClicked: callController.toggleScreenShare()
        }
        AppButton {
            text: "📞 Sair"
            baseColor: Theme.danger
            onClicked: callController.leaveCall()
        }
    }
}
