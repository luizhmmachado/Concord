import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// Shell shown once authenticated: server rail + channel list + chat/call area.
Rectangle {
    id: root
    anchors.fill: parent
    color: Theme.background

    property string activeChannelId: ""
    property string activeChannelType: ""
    property bool showSettings: false
    property bool showServerSettings: false
    property bool showDirectMessages: true
    property string bannerMessage: ""

    Component.onCompleted: serverController.loadServers(authController.userId)

    Connections {
        target: serverController
        function onErrorOccurred(message) { root.bannerMessage = message.length > 0 ? message : "Ocorreu um erro ao acessar o servidor (verifique as permiss\u00f5es/RLS no Supabase)." }
    }
    Connections {
        target: chatController
        function onErrorOccurred(message) { root.bannerMessage = message }
    }
    Connections {
        target: directMessageController
        function onErrorOccurred(message) { root.bannerMessage = message }
    }

    Rectangle {
        visible: root.bannerMessage.length > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: bannerText.implicitHeight + 16
        color: Theme.danger
        z: 20

        Text {
            id: bannerText
            anchors.left: parent.left
            anchors.right: closeBanner.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 8
            text: root.bannerMessage
            color: "white"
            wrapMode: Text.WordWrap
        }
        AppIcon {
            id: closeBanner
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 8
            name: "close"
            size: 14
            color: "white"
            MouseArea { anchors.fill: parent; onClicked: root.bannerMessage = "" }
        }
    }

    ServerSidebar {
        id: serverSidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        directMessagesActive: root.showDirectMessages
        onSettingsRequested: root.showSettings = true
        onCreateServerRequested: createServerDialog.open()
        onDirectMessagesRequested: root.showDirectMessages = true
        onServerSelected: root.showDirectMessages = false
    }

    ChannelSidebar {
        id: channelSidebar
        visible: !root.showDirectMessages
        width: visible ? 232 : 0
        anchors.left: serverSidebar.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onChannelSelected: (channelId, type) => {
            root.activeChannelId = channelId
            root.activeChannelType = type
            if (type === "text") {
                chatController.openChannel(channelId)
            } else {
                callController.startChannelCall(channelId, serverController.currentServerId)
            }
        }
    }

    MemberSidebar {
        id: memberSidebar
        visible: !root.showDirectMessages
                 && serverController.currentServerId.length > 0
                 && (!callController.inCall || root.activeChannelType === "text")
        width: visible ? 240 : 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onServerSettingsRequested: root.showServerSettings = true
    }

    Loader {
        anchors.left: channelSidebar.right
        anchors.top: parent.top
        anchors.right: memberSidebar.left
        anchors.bottom: parent.bottom
        sourceComponent: {
            if (root.showDirectMessages)
                return directMessagesComponent
            // A text channel takes precedence over the call view so the user
            // can read messages (and keep the member sidebar) while in a call.
            if (root.activeChannelType === "text")
                return chatViewComponent
            if (root.activeChannelType === "voice" || callController.inCall)
                return callViewComponent
            if (root.activeChannelId.length > 0)
                return chatViewComponent
            return emptyStateComponent
        }
    }

    Component { id: chatViewComponent; ChatView {} }
    Component { id: callViewComponent; CallView {} }
    Component { id: directMessagesComponent; DirectMessagesView {} }
    Component {
        id: emptyStateComponent
        Rectangle {
            color: Theme.background
            anchors.fill: parent
            Text {
                anchors.centerIn: parent
                text: "Selecione um canal para começar"
                color: Theme.textSecondary
                font.pixelSize: 16
            }
        }
    }

    Rectangle {
        visible: root.showSettings
        anchors.fill: parent
        color: Theme.background
        z: 10
        SettingsPage {
            id: settingsPage
            onCloseRequested: root.showSettings = false
        }
    }

    Connections {
        target: root
        function onShowSettingsChanged() {
            if (root.showSettings)
                settingsPage.refreshMediaDevices()
            else
                settingsPage.stopMediaSessions()
        }
    }

    Rectangle {
        visible: root.showServerSettings
        anchors.fill: parent
        color: Theme.background
        z: 15
        ServerSettingsPage { onCloseRequested: root.showServerSettings = false }
    }

    ImageViewer {
        anchors.fill: parent
        z: 30
        visible: imageController.viewerUrl.length > 0
        imageSource: imageController.viewerUrl
        onCloseRequested: imageController.closeImage()
    }

    ComponentPopup {
        id: createServerDialog
        title: "Servidores"
        standardButtons: Dialog.Close

        Column {
            spacing: 14

            Text { text: "Criar um servidor"; color: Theme.textPrimary; font.bold: true }
            Row {
                spacing: 8
                AppTextField { id: serverNameField; placeholderText: "Nome do servidor"; width: 240 }
                AppButton {
                    text: "Criar"
                    enabled: serverNameField.text.length > 0
                    onClicked: {
                        serverController.createServer(serverNameField.text)
                        serverNameField.text = ""
                        createServerDialog.close()
                    }
                }
            }

            Rectangle { width: 320; height: 1; color: Qt.rgba(1, 1, 1, 0.08) }

            Text { text: "Entrar com um convite"; color: Theme.textPrimary; font.bold: true }
            Row {
                spacing: 8
                AppTextField { id: inviteCodeField; placeholderText: "concord://invite/codigo"; width: 240 }
                AppButton {
                    text: "Entrar"
                    enabled: inviteCodeField.text.length > 0
                    onClicked: {
                        serverController.joinServerByInviteCode(inviteCodeField.text)
                        inviteCodeField.text = ""
                        createServerDialog.close()
                    }
                }
            }
        }
    }
}
