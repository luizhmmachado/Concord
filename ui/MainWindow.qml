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

    Component.onCompleted: serverController.loadServers(authController.userId)

    ServerSidebar {
        id: serverSidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onSettingsRequested: root.showSettings = true
        onCreateServerRequested: createServerDialog.open()
    }

    ChannelSidebar {
        id: channelSidebar
        anchors.left: serverSidebar.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onChannelSelected: (channelId, type) => {
            root.activeChannelId = channelId
            root.activeChannelType = type
            if (type === "text") {
                chatController.openChannel(channelId)
            } else {
                callController.startChannelCall(channelId)
            }
        }
    }

    Loader {
        anchors.left: channelSidebar.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        sourceComponent: {
            if (root.activeChannelType === "voice" || callController.inCall)
                return callViewComponent
            if (root.activeChannelId.length > 0)
                return chatViewComponent
            return emptyStateComponent
        }
    }

    Component { id: chatViewComponent; ChatView {} }
    Component { id: callViewComponent; CallView {} }
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
        SettingsPage { onCloseRequested: root.showSettings = false }
    }

    Dialog {
        id: createServerDialog
        title: "Novo servidor"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        Column {
            spacing: 8
            AppTextField { id: serverNameField; placeholderText: "Nome do servidor"; width: 240 }
        }

        onAccepted: {
            if (serverNameField.text.length === 0)
                return
            serverController.createServer(serverNameField.text)
            serverNameField.text = ""
        }
    }
}
