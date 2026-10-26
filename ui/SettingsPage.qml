import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia 5.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    color: Theme.background
    anchors.fill: parent

    signal closeRequested()

    property string activeSection: "account"

    Component.onCompleted: mediaDeviceController.refresh()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.surface

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "User Settings"
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: 22
                }
                Text {
                    text: "Configure your client experience"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                Rectangle { width: parent.width; height: 1; color: Qt.rgba(1,1,1,0.08) }

                Text {
                    text: "USER SETTINGS"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "account", label: "My Account" },
                        { key: "profiles", label: "Profiles" },
                        { key: "privacy", label: "Privacy & Safety" }
                    ]
                    delegate: Rectangle {
                        width: parent.width
                        height: 34
                        radius: 8
                        color: activeSection === modelData.key ? Qt.rgba(0.12, 0.82, 0.76, 0.18) : "transparent"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: activeSection = modelData.key
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            text: modelData.label
                            color: activeSection === modelData.key ? Theme.accent : Theme.textSecondary
                            font.bold: activeSection === modelData.key
                            font.pixelSize: 13
                        }
                    }
                }

                Text {
                    text: "APP SETTINGS"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "appearance", label: "Appearance" },
                        { key: "voice", label: "Voice & Video" },
                        { key: "notifications", label: "Notifications" }
                    ]
                    delegate: Rectangle {
                        width: parent.width
                        height: 34
                        radius: 8
                        color: activeSection === modelData.key ? Qt.rgba(0.12, 0.82, 0.76, 0.18) : "transparent"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: activeSection = modelData.key
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            text: modelData.label
                            color: activeSection === modelData.key ? Theme.accent : Theme.textSecondary
                            font.bold: activeSection === modelData.key
                            font.pixelSize: 13
                        }
                    }
                }

                Item { width: 1; height: 1 }

                AppButton {
                    text: "ESC  Close Settings"
                    baseColor: Theme.surfaceAlt
                    textColor: Theme.textSecondary
                    onClicked: closeRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.surfaceAlt

            Loader {
                anchors.fill: parent
                anchors.margins: 20
                sourceComponent: {
                    if (activeSection === "account") return accountSection
                    if (activeSection === "voice") return voiceSection
                    if (activeSection === "appearance") return appearanceSection
                    if (activeSection === "notifications") return notificationsSection
                    return accountSection
                }
            }
        }
    }

    Component {
        id: accountSection
        ColumnLayout {
            spacing: 16
            Text {
                text: "My Account"
                color: Theme.textPrimary
                font.pixelSize: 32
                font.bold: true
            }
            Text {
                text: "Manage your core digital identity on the Pulse network."
                color: Theme.textSecondary
                font.pixelSize: 14
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                radius: Theme.radiusMedium
                color: Theme.surface
                clip: true
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 110
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0.12, 0.82, 0.76, 0.78) }
                        GradientStop { position: 1.0; color: Qt.rgba(0.9, 0.95, 0.92, 0.92) }
                    }
                }
                Avatar {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 16
                    anchors.topMargin: 68
                    width: 70
                    height: 70
                    displayName: authController.displayName.length > 0 ? authController.displayName : "U"
                }
                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10
                    AppButton { text: "Edit User Profile"; baseColor: Theme.accent; textColor: Theme.surfaceAlt }
                    AppButton { text: "Log Out"; baseColor: Theme.surfaceAlt; textColor: Theme.textPrimary; onClicked: authController.signOut() }
                }
                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 16
                    spacing: 8
                    Text {
                        text: authController.displayName.length > 0 ? authController.displayName : "Usuario"
                        color: Theme.textPrimary
                        font.pixelSize: 28
                        font.bold: true
                    }
                    Text {
                        text: authController.username.length > 0 ? "@" + authController.username : "@sem-username"
                        color: Theme.accent
                        font.pixelSize: 14
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 12
                Rectangle { Layout.fillWidth: true; height: 54; radius: 10; color: Theme.surface; Text { anchors.centerIn: parent; text: "Theme"; color: Theme.textSecondary } }
                Rectangle { Layout.fillWidth: true; height: 54; radius: 10; color: Theme.surface; Text { anchors.centerIn: parent; text: "Accent"; color: Theme.textSecondary } }
                Rectangle { Layout.fillWidth: true; height: 54; radius: 10; color: Theme.surface; Text { anchors.centerIn: parent; text: "Notifications"; color: Theme.textSecondary } }
                Rectangle { Layout.fillWidth: true; height: 54; radius: 10; color: Theme.surface; Text { anchors.centerIn: parent; text: "Security"; color: Theme.textSecondary } }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: inviteColumn.implicitHeight + 32
                radius: Theme.radiusMedium
                color: Theme.surface

                Column {
                    id: inviteColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 16
                    spacing: 8

                    Text {
                        text: "Seu link de convite"
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Compartilhe com quem quiser te adicionar como amigo."
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }
                    Row {
                        spacing: 8
                        width: parent.width

                        AppTextField {
                            id: myInviteField
                            width: parent.width - copyInviteButton.width - 8
                            readOnly: true
                            text: "concord://user/" + authController.username
                        }
                        AppButton {
                            id: copyInviteButton
                            text: "Copiar"
                            onClicked: {
                                myInviteField.selectAll()
                                myInviteField.copy()
                                myInviteField.deselect()
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: voiceSection
        ColumnLayout {
            spacing: 16
            
            Component.onCompleted: {
                settingsCamera.stop()
                cameraStartTimer.start()
            }
            
            Component.onDestruction: {
                settingsCamera.stop()
                cameraStartTimer.stop()
            }
            
            Text {
                text: "Voice & Video"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            Rectangle {
                Layout.fillWidth: true
                height: 200
                radius: Theme.radiusMedium
                color: Theme.surface
                border.width: 1
                border.color: Qt.rgba(1,1,1,0.08)
                clip: true
                
                Camera {
                    id: settingsCamera
                }
                
                Timer {
                    id: cameraStartTimer
                    interval: 100
                    repeat: false
                    onTriggered: settingsCamera.start()
                }
                
                VideoOutput {
                    anchors.fill: parent
                    source: settingsCamera
                    fillMode: VideoOutput.PreserveAspectCrop
                }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: 12
                    text: "Camera preview"
                    color: Theme.textPrimary
                    font.bold: true
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 12
                columnSpacing: 12
                Label { text: "Microphone"; color: Theme.textSecondary }
                AppComboBox {
                    Layout.fillWidth: true
                    model: mediaDeviceController.microphones
                    currentIndex: model.indexOf(settingsController.inputDevice)
                    onActivated: settingsController.inputDevice = model[currentIndex]
                }
                Label { text: "Speaker"; color: Theme.textSecondary }
                AppComboBox {
                    Layout.fillWidth: true
                    model: mediaDeviceController.speakers
                    currentIndex: model.indexOf(settingsController.outputDevice)
                    onActivated: settingsController.outputDevice = model[currentIndex]
                }
                Label { text: "Camera"; color: Theme.textSecondary }
                AppComboBox {
                    Layout.fillWidth: true
                    model: mediaDeviceController.cameras
                    currentIndex: model.indexOf(settingsController.videoDevice)
                    onActivated: settingsController.videoDevice = model[currentIndex]
                }
            }
        }
    }

    Component {
        id: appearanceSection
        ColumnLayout {
            spacing: 16
            Text {
                text: "Appearance"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            RowLayout {
                spacing: 8
                Repeater {
                    model: ["dark", "light", "midnight"]
                    delegate: AppButton {
                        text: modelData
                        baseColor: settingsController.theme === modelData ? Theme.accent : Theme.surface
                        textColor: settingsController.theme === modelData ? Theme.surfaceAlt : Theme.textPrimary
                        onClicked: settingsController.theme = modelData
                    }
                }
            }
            Row {
                spacing: 8
                Repeater {
                    model: ["#1FD1C1", "#00A8FC", "#23A55A", "#EB459E", "#F0B232", "#DA373C"]
                    delegate: Rectangle {
                        width: 26; height: 26; radius: 13
                        color: modelData
                        border.width: settingsController.accentColor === modelData ? 2 : 0
                        border.color: Theme.textPrimary
                        MouseArea { anchors.fill: parent; onClicked: settingsController.accentColor = modelData }
                    }
                }
            }
        }
    }

    Component {
        id: notificationsSection
        ColumnLayout {
            spacing: 16
            Text {
                text: "Notifications"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            AppCheckBox {
                text: "Enable notifications"
                checked: settingsController.notificationsEnabled
                onToggled: settingsController.notificationsEnabled = checked
            }
        }
    }
}
