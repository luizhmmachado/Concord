import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/style"
import "qrc:/ui/Components"

// Full-screen server configuration, following the same layout as the user
// settings page: a fixed sidebar with the available sections and a content
// panel on the right.
Rectangle {
    id: root
    anchors.fill: parent
    color: Theme.background

    signal closeRequested()

    property string activeSection: "roles"
    property string roleColor: "#5865F2"
    property string invitePolicy: "everyone"
    property var inviteRoleIds: []
    property var inviteMemberIds: []
    property string errorMessage: ""

    function copyIds(ids) {
        var copy = []
        if (ids) {
            for (var i = 0; i < ids.length; ++i)
                copy.push(ids[i])
        }
        return copy
    }

    function toggleId(ids, id, enabled) {
        var copy = ids.slice()
        var index = copy.indexOf(id)
        if (enabled && index < 0)
            copy.push(id)
        else if (!enabled && index >= 0)
            copy.splice(index, 1)
        return copy
    }

    function permissionSummary(permissions) {
        if (!permissions)
            return "Sem permiss\u00f5es"
        var labels = []
        if (permissions.manage_roles)
            labels.push("cargos")
        if (permissions.manage_channels)
            labels.push("canais")
        if (permissions.manage_members)
            labels.push("membros")
        return labels.length > 0 ? labels.join(", ") : "Sem permiss\u00f5es"
    }

    function loadInviteSettings() {
        invitePolicy = serverMembers.invitePolicy.length > 0 ? serverMembers.invitePolicy : "everyone"
        inviteRoleIds = copyIds(serverMembers.inviteRoleIds)
        inviteMemberIds = copyIds(serverMembers.inviteMemberIds)
    }

    function selectSection(section) {
        activeSection = section
        if (section === "invites") {
            loadInviteSettings()
            serverMembers.refreshInviteSettings()
        }
    }

    function saveInviteSettings() {
        serverMembers.saveInviteSettings(invitePolicy, inviteRoleIds, inviteMemberIds)
    }

    function ensureSectionAvailable() {
        if (activeSection === "roles" && !serverMembers.canManageRoles && serverMembers.canManageMembers)
            activeSection = "invites"
        else if (activeSection === "invites" && !serverMembers.canManageMembers && serverMembers.canManageRoles)
            activeSection = "roles"
    }

    Component.onCompleted: {
        loadInviteSettings()
        ensureSectionAvailable()
    }

    onVisibleChanged: {
        if (visible) {
            loadInviteSettings()
            ensureSectionAvailable()
        }
    }

    Connections {
        target: serverMembers
        function onErrorOccurred(message) { root.errorMessage = message }
        function onCanManageRolesChanged() { root.ensureSectionAvailable() }
        function onCanManageMembersChanged() { root.ensureSectionAvailable() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 18

        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: "Configura\u00e7\u00f5es"
                    color: Theme.textPrimary
                    font.bold: true
                    font.pixelSize: 22
                }

                Text {
                    Layout.fillWidth: true
                    text: serverController.currentServerName
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                Text {
                    Layout.fillWidth: true
                    text: "SERVIDOR"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "roles", label: "Cargos", available: serverMembers.canManageRoles },
                        { key: "invites", label: "Convites", available: serverMembers.canManageMembers }
                    ]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: 8
                        visible: modelData.available
                        color: root.activeSection === modelData.key
                               ? Qt.rgba(0.12, 0.82, 0.76, 0.18) : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectSection(modelData.key)
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
                    visible: root.errorMessage.length > 0
                    text: root.errorMessage
                    color: Theme.danger
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                AppButton {
                    Layout.fillWidth: true
                    text: "ESC  Voltar"
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

            Loader {
                anchors.fill: parent
                anchors.margins: 20
                sourceComponent: root.activeSection === "invites" ? invitesSection : rolesSection
            }
        }
    }

    Component {
        id: rolesSection

        ColumnLayout {
            spacing: 14

            Text {
                text: "Cargos"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                text: "Crie cargos e defina quem pode gerenciar o servidor."
                color: Theme.textSecondary
                font.pixelSize: 13
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                color: Theme.surface
                clip: true

                Text {
                    anchors.centerIn: parent
                    visible: serverMembers.roleCount === 0
                    text: "Nenhum cargo criado ainda."
                    color: Theme.textSecondary
                }

                ListView {
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true
                    spacing: 6
                    model: serverMembers.roles

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 48
                        radius: Theme.radiusSmall
                        color: Theme.surfaceAlt

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 14
                                height: 14
                                radius: 7
                                color: model.color.length > 0 ? model.color : Theme.accent
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 130
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: model.name
                                    color: Theme.textPrimary
                                    font.bold: true
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: root.permissionSummary(model.permissions)
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            AppButton {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: serverMembers.canManageRoles
                                         && serverMembers.canGrantPermissions(model.permissions)
                                text: "Excluir"
                                height: 30
                                implicitHeight: 30
                                baseColor: Theme.danger
                                onClicked: serverMembers.deleteRole(model.roleId)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: createRoleColumn.implicitHeight + 28
                radius: Theme.radiusMedium
                color: Theme.surface

                Column {
                    id: createRoleColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "Criar cargo"
                        color: Theme.textPrimary
                        font.bold: true
                        font.pixelSize: 15
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 10

                        AppTextField {
                            id: roleNameField
                            Layout.fillWidth: true
                            placeholderText: "Nome do cargo"
                        }

                        Rectangle {
                            Layout.preferredWidth: 170
                            Layout.preferredHeight: 40
                            radius: Theme.radiusSmall
                            color: Theme.surfaceAlt
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            Row {
                                anchors.centerIn: parent
                                spacing: 8

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: root.roleColor
                                    border.width: 1
                                    border.color: Qt.rgba(1, 1, 1, 0.2)
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.roleColor.toUpperCase()
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    colorPicker.initialColor = root.roleColor
                                    colorPicker.open()
                                }
                            }
                        }
                    }

                    RowLayout {
                        width: parent.width

                        AppCheckBox {
                            id: manageRolesCheck
                            text: "Gerenciar cargos"
                            enabled: serverMembers.canManageRoles
                        }
                        AppCheckBox {
                            id: manageChannelsCheck
                            text: "Gerenciar canais"
                            enabled: serverMembers.canManageChannels
                        }
                        AppCheckBox {
                            id: manageMembersCheck
                            text: "Gerenciar membros"
                            enabled: serverMembers.canManageMembers
                        }
                    }

                    AppButton {
                        text: "Criar cargo"
                        enabled: roleNameField.text.trim().length > 0 && serverMembers.canManageRoles
                        onClicked: {
                            serverMembers.createRole(roleNameField.text.trim(), root.roleColor, {
                                "manage_roles": manageRolesCheck.checked,
                                "manage_channels": manageChannelsCheck.checked,
                                "manage_members": manageMembersCheck.checked
                            })
                            roleNameField.text = ""
                            root.roleColor = "#5865F2"
                            manageRolesCheck.checked = false
                            manageChannelsCheck.checked = false
                            manageMembersCheck.checked = false
                        }
                    }
                }
            }
        }
    }

    Component {
        id: invitesSection

        ColumnLayout {
            spacing: 14

            Text {
                text: "Convites"
                color: Theme.textPrimary
                font.pixelSize: 28
                font.bold: true
            }

            Text {
                text: "Escolha quem pode criar convites para este servidor."
                color: Theme.textSecondary
                font.pixelSize: 13
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                radius: Theme.radiusMedium
                color: Theme.surface

                Row {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 24

                    AppRadioButton {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Todos os membros"
                        checked: root.invitePolicy === "everyone"
                        onToggled: if (checked) root.invitePolicy = "everyone"
                    }

                    AppRadioButton {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Somente selecionados"
                        checked: root.invitePolicy === "restricted"
                        onToggled: if (checked) root.invitePolicy = "restricted"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: serverMembers.inviteSettingsLoading
                        text: "Carregando configura\u00e7\u00e3o..."
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                color: Theme.surface
                clip: true
                opacity: root.invitePolicy === "restricted" ? 1 : 0.5

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "Cargos permitidos"
                            color: Theme.textPrimary
                            font.bold: true
                        }

                        Text {
                            visible: serverMembers.roleCount === 0
                            text: "Nenhum cargo criado ainda."
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: serverMembers.roles

                            delegate: AppCheckBox {
                                width: ListView.view.width
                                text: model.name
                                enabled: root.invitePolicy === "restricted"
                                checked: root.inviteRoleIds.indexOf(model.roleId) >= 0
                                onToggled: root.inviteRoleIds =
                                    root.toggleId(root.inviteRoleIds, model.roleId, checked)
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        width: 1
                        color: Qt.rgba(1, 1, 1, 0.08)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "Membros permitidos"
                            color: Theme.textPrimary
                            font.bold: true
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 4
                            model: serverMembers.allMembers

                            delegate: AppCheckBox {
                                width: ListView.view.width
                                text: modelData.displayName.length > 0
                                      ? modelData.displayName : modelData.username
                                enabled: root.invitePolicy === "restricted"
                                checked: root.inviteMemberIds.indexOf(modelData.userId) >= 0
                                onToggled: root.inviteMemberIds =
                                    root.toggleId(root.inviteMemberIds, modelData.userId, checked)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                AppButton {
                    text: "Salvar convites"
                    enabled: serverMembers.canManageMembers
                    onClicked: root.saveInviteSettings()
                }
            }
        }
    }

    ColorPickerDialog {
        id: colorPicker
        initialColor: root.roleColor
        onAccepted: root.roleColor = colorPicker.selectedColor
    }
}
