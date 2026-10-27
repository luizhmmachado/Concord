import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtMultimedia 5.15
import "qrc:/style"
import "qrc:/ui/Components"

Rectangle {
    id: root
    color: Theme.background
    anchors.fill: parent

    signal closeRequested()

    property string activeSection: "account"

    function deviceIndex(list, value) {
        if (!list)
            return -1
        for (var i = 0; i < list.length; ++i) {
            if (list[i] === value)
                return i
        }
        return -1
    }

    function ensureDeviceDefaults() {
        if (mediaDeviceController.microphones.length > 0
                && deviceIndex(mediaDeviceController.microphones, settingsController.inputDevice) < 0)
            settingsController.inputDevice = mediaDeviceController.microphones[0]
        if (mediaDeviceController.speakers.length > 0
                && deviceIndex(mediaDeviceController.speakers, settingsController.outputDevice) < 0)
            settingsController.outputDevice = mediaDeviceController.speakers[0]
        if (mediaDeviceController.cameras.length > 0
                && deviceIndex(mediaDeviceController.cameras, settingsController.videoDevice) < 0)
            settingsController.videoDevice = mediaDeviceController.cameras[0]
    }

    Component.onCompleted: {
        mediaDeviceController.refresh()
        ensureDeviceDefaults()
    }
    onActiveSectionChanged: contentFlick.contentY = 0

    Connections {
        target: mediaDeviceController
        function onDevicesChanged() { root.ensureDeviceDefaults() }
    }
    Connections {
        target: settingsController
        function onInputDeviceChanged() { root.ensureDeviceDefaults() }
        function onOutputDeviceChanged() { root.ensureDeviceDefaults() }
        function onVideoDeviceChanged() { root.ensureDeviceDefaults() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "Configurações"
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: 22
                }
                Text {
                    Layout.fillWidth: true
                    text: "Gerencie sua conta e preferências"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    Layout.bottomMargin: 6
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                Text {
                    Layout.fillWidth: true
                    text: "CONTA"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "account", label: "Minha conta" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: 8
                        color: root.activeSection === modelData.key
                               ? Qt.rgba(0.12, 0.82, 0.76, 0.18) : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSection = modelData.key
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: modelData.label
                            color: root.activeSection === modelData.key ? Theme.accent : Theme.textSecondary
                            font.bold: root.activeSection === modelData.key
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 10
                    text: "APLICATIVO"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "appearance", label: "Aparência" },
                        { key: "voice", label: "Voz e vídeo" },
                        { key: "notifications", label: "Notificações" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: 8
                        color: root.activeSection === modelData.key
                               ? Qt.rgba(0.12, 0.82, 0.76, 0.18) : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSection = modelData.key
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: modelData.label
                            color: root.activeSection === modelData.key ? Theme.accent : Theme.textSecondary
                            font.bold: root.activeSection === modelData.key
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                AppButton {
                    Layout.fillWidth: true
                    text: "ESC  Fechar"
                    baseColor: Theme.surfaceAlt
                    textColor: Theme.textSecondary
                    onClicked: root.closeRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.surfaceAlt
            clip: true

            Flickable {
                id: contentFlick
                anchors.fill: parent
                anchors.margins: 20
                contentWidth: width
                contentHeight: contentLoader.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Loader {
                    id: contentLoader
                    width: parent.width
                    sourceComponent: {
                        if (root.activeSection === "voice") return voiceSection
                        if (root.activeSection === "appearance") return appearanceSection
                        if (root.activeSection === "notifications") return notificationsSection
                        return accountSection
                    }
                }
            }
        }
    }

    Component {
        id: accountSection
        ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: "Minha conta"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: "Gerencie sua identidade e as informações exibidas para os outros usuários."
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                radius: Theme.radiusMedium
                color: Theme.surface
                clip: true

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 110
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0.12, 0.82, 0.76, 0.62) }
                        GradientStop { position: 1.0; color: Theme.surface }
                    }
                }

                AppButton {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 16
                    height: 34
                    implicitHeight: 34
                    text: "Sair"
                    baseColor: Theme.surfaceAlt
                    textColor: Theme.textPrimary
                    onClicked: authController.signOut()
                }

                Avatar {
                    id: profileAvatar
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 16
                    anchors.bottomMargin: 16
                    width: 72
                    height: 72
                    displayName: authController.displayName.length > 0 ? authController.displayName : "U"
                }

                Column {
                    anchors.left: profileAvatar.right
                    anchors.leftMargin: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    spacing: 4

                    Text {
                        width: parent.width
                        text: authController.displayName.length > 0 ? authController.displayName : "Usuário"
                        color: Theme.textPrimary
                        font.pixelSize: 26
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: authController.username.length > 0 ? "@" + authController.username : "@sem-username"
                        color: Theme.accent
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: [
                        { key: "appearance", title: "Aparência", detail: "Tema e cor de destaque" },
                        { key: "notifications", title: "Notificações", detail: "Alertas e sons" }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 66
                        radius: 10
                        color: shortcutMouse.containsMouse ? Theme.surfaceAlt : Theme.surface

                        Column {
                            anchors.left: parent.left
                            anchors.right: shortcutChevron.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 16
                            anchors.rightMargin: 12
                            spacing: 2

                            Text {
                                width: parent.width
                                text: modelData.title
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: modelData.detail
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }

                        AppIcon {
                            id: shortcutChevron
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: 14
                            name: "chevron-down"
                            rotation: -90
                            size: 14
                            color: Theme.textSecondary
                        }

                        MouseArea {
                            id: shortcutMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeSection = modelData.key
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: inviteLayout.implicitHeight + 32
                radius: Theme.radiusMedium
                color: Theme.surface

                ColumnLayout {
                    id: inviteLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "Seu link de convite"
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Compartilhe com quem quiser te adicionar como amigo."
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        AppTextField {
                            id: myInviteField
                            Layout.fillWidth: true
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
            id: voiceRoot
            property bool previewEnabled: false

            spacing: 18

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: "Voz e vídeo"
                    color: Theme.textPrimary
                    font.pixelSize: 28
                    font.bold: true
                }

                AppButton {
                    text: voiceRoot.previewEnabled ? "Desligar prévia" : "Ligar prévia"
                    iconName: voiceRoot.previewEnabled ? "camera-off" : "camera-on"
                    baseColor: voiceRoot.previewEnabled ? Theme.surfaceAlt : Theme.accent
                    textColor: voiceRoot.previewEnabled ? Theme.textPrimary : Theme.surfaceAlt
                    onClicked: voiceRoot.previewEnabled = !voiceRoot.previewEnabled
                }
            }
            Text {
                Layout.fillWidth: true
                text: "Escolha seus dispositivos e ligue a prévia apenas quando quiser conferir a câmera."
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                radius: Theme.radiusMedium
                color: Theme.surface
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)
                clip: true

                Rectangle {
                    anchors.fill: parent
                    visible: !voiceRoot.previewEnabled
                    color: Theme.surfaceAlt

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Prévia desativada"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "A câmera não será acessada até você ligar a prévia."
                            color: Theme.textSecondary
                            font.pixelSize: 12
                        }
                    }
                }

                Loader {
                    id: previewLoader
                    anchors.fill: parent
                    active: voiceRoot.previewEnabled && mediaDeviceController.cameras.length > 0
                    sourceComponent: cameraPreviewComponent
                }

                Text {
                    anchors.centerIn: parent
                    visible: voiceRoot.previewEnabled && mediaDeviceController.cameras.length === 0
                    text: "Nenhuma câmera encontrada"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    visible: voiceRoot.previewEnabled && mediaDeviceController.cameras.length > 0
                    height: cameraCaption.implicitHeight + 10
                    width: cameraCaption.implicitWidth + 16
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.55)

                    Text {
                        id: cameraCaption
                        anchors.centerIn: parent
                        text: "Prévia da câmera"
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: deviceLayout.implicitHeight + 32
                radius: Theme.radiusMedium
                color: Theme.surface

                ColumnLayout {
                    id: deviceLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Dispositivos"
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            Layout.preferredWidth: 110
                            text: "Microfone"
                            color: Theme.textSecondary
                            font.pixelSize: 13
                        }
                        AppComboBox {
                            Layout.fillWidth: true
                            model: mediaDeviceController.microphones
                            currentIndex: root.deviceIndex(model, settingsController.inputDevice)
                            placeholderText: "Nenhum dispositivo encontrado"
                            onActivated: if (currentIndex >= 0) settingsController.inputDevice = model[currentIndex]
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            Layout.preferredWidth: 110
                            text: "Alto-falante"
                            color: Theme.textSecondary
                            font.pixelSize: 13
                        }
                        AppComboBox {
                            Layout.fillWidth: true
                            model: mediaDeviceController.speakers
                            currentIndex: root.deviceIndex(model, settingsController.outputDevice)
                            placeholderText: "Nenhum dispositivo encontrado"
                            onActivated: if (currentIndex >= 0) settingsController.outputDevice = model[currentIndex]
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            Layout.preferredWidth: 110
                            text: "Câmera"
                            color: Theme.textSecondary
                            font.pixelSize: 13
                        }
                        AppComboBox {
                            Layout.fillWidth: true
                            model: mediaDeviceController.cameras
                            currentIndex: root.deviceIndex(model, settingsController.videoDevice)
                            placeholderText: "Nenhum dispositivo encontrado"
                            onActivated: if (currentIndex >= 0) settingsController.videoDevice = model[currentIndex]
                        }
                    }
                }
            }
        }
    }


    Component {
        id: cameraPreviewComponent
        Item {
            id: previewRoot
            anchors.fill: parent

            property string selectedCameraId: {
                var index = root.deviceIndex(mediaDeviceController.cameras, settingsController.videoDevice)
                if (index < 0 || index >= mediaDeviceController.cameraDeviceIds.length)
                    return ""
                return mediaDeviceController.cameraDeviceIds[index]
            }

            Camera {
                id: previewCamera
                deviceId: previewRoot.selectedCameraId
            }

            VideoOutput {
                anchors.fill: parent
                source: previewCamera
                fillMode: VideoOutput.PreserveAspectCrop
            }

            Component.onCompleted: previewCamera.start()
            Component.onDestruction: previewCamera.stop()
        }
    }

    Component {
        id: appearanceSection
        ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: "Aparência"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: "Personalize o tema e a cor de destaque do aplicativo."
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: themeLayout.implicitHeight + 32
                radius: Theme.radiusMedium
                color: Theme.surface

                ColumnLayout {
                    id: themeLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Tema"
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 8

                        Repeater {
                            model: [
                                { key: "dark", label: "Escuro" },
                                { key: "light", label: "Claro" },
                                { key: "midnight", label: "Meia-noite" }
                            ]
                            delegate: AppButton {
                                text: modelData.label
                                baseColor: settingsController.theme === modelData.key ? Theme.accent : Theme.surfaceAlt
                                textColor: settingsController.theme === modelData.key ? Theme.surfaceAlt : Theme.textPrimary
                                onClicked: settingsController.theme = modelData.key
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: accentLayout.implicitHeight + 32
                radius: Theme.radiusMedium
                color: Theme.surface

                ColumnLayout {
                    id: accentLayout
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: "Cor de destaque"
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Row {
                        spacing: 10

                        Repeater {
                            model: ["#1FD1C1", "#00A8FC", "#23A55A", "#EB459E", "#F0B232", "#DA373C"]
                            delegate: Rectangle {
                                width: 28
                                height: 28
                                radius: 14
                                color: modelData
                                border.width: settingsController.accentColor === modelData ? 2 : 0
                                border.color: Theme.textPrimary

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsController.accentColor = modelData
                                }
                            }
                        }
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
                Layout.fillWidth: true
                text: "Notificações"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: "Controle os alertas exibidos pelo aplicativo."
                color: Theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                radius: Theme.radiusMedium
                color: Theme.surface

                AppCheckBox {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 16
                    text: "Ativar notificações"
                    checked: settingsController.notificationsEnabled
                    onToggled: settingsController.notificationsEnabled = checked
                }
            }
        }
    }
}
