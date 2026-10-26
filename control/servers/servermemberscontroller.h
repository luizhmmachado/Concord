#ifndef SERVERMEMBERSCONTROLLER_H
#define SERVERMEMBERSCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "servermembermodel.h"
#include "serverrolemodel.h"

class OnlinePresenceController;

// Loads the members and roles of the selected server and keeps the member list
// live. Online/offline status comes from OnlinePresenceController; roles and
// assignments are persisted in server_roles / server_member_roles and guarded
// by RLS using the manage_roles permission.
class ServerMembersController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ServerMemberModel *members READ members CONSTANT)
    Q_PROPERTY(ServerRoleModel *roles READ roles CONSTANT)
    Q_PROPERTY(QVariantList onlineMembers READ onlineMembers NOTIFY membersChanged)
    Q_PROPERTY(QVariantList offlineMembers READ offlineMembers NOTIFY membersChanged)
    Q_PROPERTY(QVariantList allMembers READ allMembers NOTIFY membersChanged)
    Q_PROPERTY(bool canManageRoles READ canManageRoles NOTIFY canManageRolesChanged)
    Q_PROPERTY(bool canManageChannels READ canManageChannels NOTIFY canManageChannelsChanged)
    Q_PROPERTY(bool canManageMembers READ canManageMembers NOTIFY canManageMembersChanged)
    Q_PROPERTY(bool canInvite READ canInvite NOTIFY canInviteChanged)
    Q_PROPERTY(QString invitePolicy READ invitePolicy NOTIFY inviteSettingsChanged)
    Q_PROPERTY(QVariantList inviteRoleIds READ inviteRoleIds NOTIFY inviteSettingsChanged)
    Q_PROPERTY(QVariantList inviteMemberIds READ inviteMemberIds NOTIFY inviteSettingsChanged)
    Q_PROPERTY(bool inviteSettingsLoading READ inviteSettingsLoading NOTIFY inviteSettingsLoadingChanged)
    Q_PROPERTY(int roleCount READ roleCount NOTIFY rolesChanged)
    Q_PROPERTY(QString currentServerId READ currentServerId NOTIFY currentServerIdChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    ServerMembersController(SupabaseClient *client, RealtimeClient *realtime,
                            OnlinePresenceController *onlinePresence,
                            QObject *parent = nullptr);

    ServerMemberModel *members() { return &m_members; }
    ServerRoleModel *roles() { return &m_roles; }
    QVariantList onlineMembers() const { return memberList(true); }
    QVariantList offlineMembers() const { return memberList(false); }
    QVariantList allMembers() const {
        QVariantList list = memberList(true);
        list.append(memberList(false));
        return list;
    }
    bool canManageRoles() const { return m_canManageRoles; }
    bool canManageChannels() const { return m_canManageChannels; }
    bool canManageMembers() const { return m_canManageMembers; }
    bool canInvite() const { return m_canInvite; }
    QString invitePolicy() const { return m_invitePolicy; }
    QVariantList inviteRoleIds() const { return m_inviteRoleIds; }
    QVariantList inviteMemberIds() const { return m_inviteMemberIds; }
    bool inviteSettingsLoading() const { return m_inviteSettingsLoading; }
    int roleCount() const { return m_roles.rowCount(); }
    QString currentServerId() const { return m_currentServerId; }
    bool loading() const { return m_loading; }

    Q_INVOKABLE void watchServer(const QString &serverId);
    Q_INVOKABLE void createRole(const QString &name, const QString &color,
                                const QVariantMap &permissions);
    Q_INVOKABLE void deleteRole(const QString &roleId);
    Q_INVOKABLE void setRoleAssigned(const QString &userId, const QString &roleId, bool assigned);
    Q_INVOKABLE void removeMember(const QString &userId);
    Q_INVOKABLE bool canGrantPermissions(const QVariantMap &permissions) const;
    Q_INVOKABLE void refreshInviteSettings();
    Q_INVOKABLE void saveInviteSettings(const QString &policy, const QVariantList &roleIds,
                                        const QVariantList &memberIds);

signals:
    void membersChanged();
    void canManageRolesChanged();
    void canManageChannelsChanged();
    void canManageMembersChanged();
    void canInviteChanged();
    void inviteSettingsChanged();
    void inviteSettingsLoadingChanged();
    void rolesChanged();
    void currentServerIdChanged();
    void loadingChanged();
    void errorOccurred(const QString &message);

private:
    static QVariantMap memberToMap(const ServerMemberEntry &entry);
    QVariantList memberList(bool online) const;

    void subscribeRealtime();
    void refreshAll();
    void refreshMembers();
    void refreshRoles();
    void resetInviteSettings();
    void applyInviteSettings(const QJsonObject &settings);
    void setInviteSettingsLoading(bool loading);
    bool selfHasPermission(const QString &permission) const;
    void updatePermissions();
    void updateLoading();
    void setMembersLoading(bool loading);
    void setRolesLoading(bool loading);

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    OnlinePresenceController *m_onlinePresence;
    ServerMemberModel m_members;
    ServerRoleModel m_roles;

    QString m_currentServerId;
    bool m_canManageRoles = false;
    bool m_canManageChannels = false;
    bool m_canManageMembers = false;
    bool m_canInvite = false;
    QString m_invitePolicy;
    QVariantList m_inviteRoleIds;
    QVariantList m_inviteMemberIds;
    bool m_membersLoading = false;
    bool m_rolesLoading = false;
    bool m_inviteSettingsLoading = false;
    bool m_loading = false;
    bool m_realtimeSubscribed = false;
    int m_membersRequestId = 0;
    int m_rolesRequestId = 0;
    int m_inviteSettingsRequestId = 0;
    QTimer m_refreshTimer;
};

#endif // SERVERMEMBERSCONTROLLER_H
