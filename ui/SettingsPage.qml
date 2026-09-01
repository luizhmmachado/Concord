import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Single screen with every user-configurable setting: theme, accent color,
// notifications and input/output/camera device selection.
Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal closeRequested()

    Component.onCompleted: mediaDeviceController.refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Configurações"; color: Theme.textPrimary; font.pixelSize: 24; font.bold: true; Layout.fillWidth: true }
            AppButton { text: "Fechar"; onClicked: closeRequested() }
        }

        Text { text: "Tema"; color: Theme.textSecondary; font.bold: true }
        RowLayout {
            spacing: 8
            Repeater {
                model: ["dark", "light", "midnight"]
                delegate: AppButton {
                    text: modelData
                    baseColor: settingsController.theme === modelData ? Theme.accent : Theme.surface
                    onClicked: settingsController.theme = modelData
                }
            }
        }

        Text { text: "Cor de destaque"; color: Theme.textSecondary; font.bold: true }
        RowLayout {
            spacing: 8
            Repeater {
                model: ["#1FD1C1", "#5865F2", "#23A55A", "#EB459E", "#F0B232", "#DA373C"]
                delegate: Rectangle {
                    width: 32; height: 32; radius: 16
                    color: modelData
                    border.width: settingsController.accentColor === modelData ? 3 : 0
                    border.color: Theme.textPrimary
                    MouseArea { anchors.fill: parent; onClicked: settingsController.accentColor = modelData }
                }
            }
        }

        RowLayout {
            CheckBox {
                text: "Ativar notificações"
                checked: settingsController.notificationsEnabled
                onToggled: settingsController.notificationsEnabled = checked
            }
        }

        Text { text: "Microfone"; color: Theme.textSecondary; font.bold: true }
        ComboBox {
            model: mediaDeviceController.microphones
            currentIndex: model.indexOf(settingsController.inputDevice)
            onActivated: settingsController.inputDevice = model[currentIndex]
        }

        Text { text: "Saída de áudio"; color: Theme.textSecondary; font.bold: true }
        ComboBox {
            model: mediaDeviceController.speakers
            currentIndex: model.indexOf(settingsController.outputDevice)
            onActivated: settingsController.outputDevice = model[currentIndex]
        }

        Text { text: "Câmera"; color: Theme.textSecondary; font.bold: true }
        ComboBox {
            model: mediaDeviceController.cameras
            currentIndex: model.indexOf(settingsController.videoDevice)
            onActivated: settingsController.videoDevice = model[currentIndex]
        }

        Item { Layout.fillHeight: true }

        AppButton {
            text: "Sair da conta"
            baseColor: Theme.danger
            onClicked: authController.signOut()
        }
    }
}
