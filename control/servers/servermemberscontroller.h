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
    Q_PROPERTY(bool canManageRoles READ canManageRoles NOTIFY canManageRolesChanged)
    Q_PROPERTY(bool canManageChannels READ canManageChannels NOTIFY canManageChannelsChanged)
    Q_PROPERTY(bool canManageMembers READ canManageMembers NOTIFY canManageMembersChanged)
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
    bool canManageRoles() const { return m_canManageRoles; }
    bool canManageChannels() const { return m_canManageChannels; }
    bool canManageMembers() const { return m_canManageMembers; }
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

signals:
    void membersChanged();
    void canManageRolesChanged();
    void canManageChannelsChanged();
    void canManageMembersChanged();
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
    bool m_membersLoading = false;
    bool m_rolesLoading = false;
    bool m_loading = false;
    bool m_realtimeSubscribed = false;
    int m_membersRequestId = 0;
    int m_rolesRequestId = 0;
    QTimer m_refreshTimer;
};

#endif // SERVERMEMBERSCONTROLLER_H
