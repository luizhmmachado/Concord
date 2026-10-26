import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/style"
import "qrc:/ui/Components"

// Right column: online/offline members of the selected server, plus the entry
// point to the server settings page for users that manage roles or members.
Rectangle {
    id: root
    width: 240
    color: Theme.surface

    signal serverSettingsRequested()

    property string errorMessage: ""

    function memberHasRole(roles, roleId) {
        if (!roles)
            return false
        for (var i = 0; i < roles.length; ++i) {
            if (roles[i].id === roleId)
                return true
        }
        return false
    }

    function copyRoles(roles) {
        var copy = []
        if (roles) {
            for (var i = 0; i < roles.length; ++i)
                copy.push(roles[i])
        }
        return copy
    }

    function openMemberRoles(member) {
        if (!serverMembers.canManageRoles && !serverMembers.canManageMembers)
            return
        memberRolesDialog.memberUserId = member.userId
        memberRolesDialog.memberUserName = member.displayName.length > 0 ? member.displayName : member.username
        memberRolesDialog.memberRole = member.memberRole
        memberRolesDialog.memberRoles = copyRoles(member.roles)
        memberRolesDialog.open()
    }

    function setMemberRole(roleId, assigned) {
        var roles = copyRoles(memberRolesDialog.memberRoles)
        var filtered = []
        for (var i = 0; i < roles.length; ++i) {
            if (roles[i].id !== roleId)
                filtered.push(roles[i])
        }
        if (assigned)
            filtered.push({"id": roleId})
        memberRolesDialog.memberRoles = filtered
        serverMembers.setRoleAssigned(memberRolesDialog.memberUserId, roleId, assigned)
    }

    Connections {
        target: serverController
        function onCurrentServerChanged() { serverMembers.watchServer(serverController.currentServerId) }
    }

    Connections {
        target: serverMembers
        function onErrorOccurred(message) { root.errorMessage = message }
    }

    Component.onCompleted: serverMembers.watchServer(serverController.currentServerId)

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Row {
            id: header
            width: parent.width
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                       - (settingsButton.visible ? settingsButton.width + parent.spacing : 0)
                text: "MEMBROS"
                color: Theme.textSecondary
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            AppButton {
                id: settingsButton
                visible: serverMembers.canManageRoles || serverMembers.canManageMembers
                text: "Configura\u00e7\u00f5es"
                height: 28
                implicitHeight: 28
                baseColor: Theme.surfaceAlt
                onClicked: root.serverSettingsRequested()
            }
        }

        Flickable {
            width: parent.width
            height: parent.height - header.height - errorText.height - parent.spacing * 2
            clip: true
            contentWidth: width
            contentHeight: contentColumn.implicitHeight

            Column {
                id: contentColumn
                width: parent.width
                spacing: 2

                Text {
                    visible: serverMembers.loading
                             && serverMembers.onlineMembers.length === 0
                             && serverMembers.offlineMembers.length === 0
                    text: "Carregando membros..."
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }

                Text {
                    visible: serverMembers.onlineMembers.length > 0
                    text: "ONLINE \u2014 " + serverMembers.onlineMembers.length
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 10
                    topPadding: 4
                    bottomPadding: 2
                }

                Repeater {
                    model: serverMembers.onlineMembers
                    delegate: MemberListItem {
                        displayName: modelData.displayName
                        username: modelData.username
                        userId: modelData.userId
                        avatarUrl: modelData.avatarUrl
                        online: true
                        roleName: modelData.highestRoleName
                        roleColor: modelData.highestRoleColor
                        onClicked: root.openMemberRoles(modelData)
                    }
                }

                Text {
                    visible: serverMembers.offlineMembers.length > 0
                    text: "OFFLINE \u2014 " + serverMembers.offlineMembers.length
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 10
                    topPadding: 10
                    bottomPadding: 2
                }

                Repeater {
                    model: serverMembers.offlineMembers
                    delegate: MemberListItem {
                        displayName: modelData.displayName
                        username: modelData.username
                        userId: modelData.userId
                        avatarUrl: modelData.avatarUrl
                        online: false
                        roleName: modelData.highestRoleName
                        roleColor: modelData.highestRoleColor
                        opacity: 0.65
                        onClicked: root.openMemberRoles(modelData)
                    }
                }

                Text {
                    visible: !serverMembers.loading
                             && serverMembers.onlineMembers.length === 0
                             && serverMembers.offlineMembers.length === 0
                    text: serverController.currentServerId.length > 0 ? "Nenhum membro" : ""
                    color: Theme.textSecondary
                    font.pixelSize: 11
                }
            }
        }

        Text {
            id: errorText
            visible: root.errorMessage.length > 0
            height: visible ? implicitHeight : 0
            width: parent.width
            text: root.errorMessage
            color: Theme.danger
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }

    ComponentPopup {
        id: memberRolesDialog
        width: 360
        standardButtons: Dialog.Close

        property string memberUserId: ""
        property string memberUserName: ""
        property string memberRole: ""
        property var memberRoles: []

        title: "Gerenciar " + memberUserName

        Column {
            width: 320
            spacing: 6

            Text {
                visible: !serverMembers.canManageRoles
                text: "Voc\u00ea n\u00e3o tem permiss\u00e3o para gerenciar cargos."
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Text {
                visible: serverMembers.canManageRoles && serverMembers.roleCount === 0
                text: "Nenhum cargo criado ainda. Crie um cargo primeiro."
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                width: parent.width
            }

            Repeater {
                model: serverMembers.canManageRoles ? serverMembers.roles : []
                delegate: AppCheckBox {
                    width: 320
                    text: model.name
                    enabled: serverMembers.canGrantPermissions(model.permissions)
                    checked: root.memberHasRole(memberRolesDialog.memberRoles, model.roleId)
                    onToggled: root.setMemberRole(model.roleId, checked)
                }
            }

            AppButton {
                width: 320
                visible: serverMembers.canManageMembers
                         && memberRolesDialog.memberUserId !== authController.userId
                         && memberRolesDialog.memberRole !== "owner"
                text: "Remover do servidor"
                baseColor: Theme.danger
                onClicked: {
                    confirmRemoveDialog.memberUserId = memberRolesDialog.memberUserId
                    confirmRemoveDialog.memberUserName = memberRolesDialog.memberUserName
                    confirmRemoveDialog.open()
                }
            }
        }
    }

    ComponentPopup {
        id: confirmRemoveDialog
        title: "Remover membro"
        width: 360
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string memberUserId: ""
        property string memberUserName: ""

        Text {
            width: 300
            text: "Remover " + memberUserName + " do servidor?"
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            serverMembers.removeMember(memberUserId)
            memberRolesDialog.close()
        }
    }
}
