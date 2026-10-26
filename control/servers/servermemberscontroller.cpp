#include "servermemberscontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <QDebug>

#include "control/presence/onlinepresencecontroller.h"

namespace {
const QString kRealtimeTopic = QStringLiteral("realtime:concord:members");
}

ServerMembersController::ServerMembersController(SupabaseClient *client,
                                                 RealtimeClient *realtime,
                                                 OnlinePresenceController *onlinePresence,
                                                 QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
    , m_onlinePresence(onlinePresence)
{
    qInfo().noquote() << "ServerMembersController::ServerMembersController";

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(250);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ServerMembersController::refreshAll);

    connect(m_onlinePresence, &OnlinePresenceController::onlineUsersChanged, this, [this]() {
        m_members.setOnlineUsers(m_onlinePresence->onlineUsers());
        emit membersChanged();
    });

    connect(m_realtime, &RealtimeClient::postgresChange, this,
            [this](const QString &topic, const QString &eventType, const QJsonObject &record) {
                Q_UNUSED(eventType)
                Q_UNUSED(record)
                if (topic != kRealtimeTopic || m_currentServerId.isEmpty())
                    return;
                // The signal carries no table name, and a role/member change in
                // any server is rare enough that re-reading this server is
                // cheaper than tracking per-table filters client side.
                m_refreshTimer.start();
            });

    connect(m_client, &SupabaseClient::selectFinished, this,
            [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
                const QString prefix = QStringLiteral("members:roles:");
                if (!tag.startsWith(prefix))
                    return;
                if (tag.mid(prefix.size()).toInt() != m_rolesRequestId)
                    return;
                setRolesLoading(false);
                if (!ok) {
                    emit errorOccurred(error);
                    return;
                }
                m_roles.setRoles(rows);
                emit rolesChanged();
                updatePermissions();
            });

    connect(m_client, &SupabaseClient::rpcFinished, this,
            [this](const QString &tag, bool ok, const QJsonValue &result, const QString &error) {
                const QString listPrefix = QStringLiteral("members:list:");
                if (tag.startsWith(listPrefix)) {
                    if (tag.mid(listPrefix.size()).toInt() != m_membersRequestId)
                        return;
                    setMembersLoading(false);
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    m_members.setMembers(result.toArray());
                    m_members.setOnlineUsers(m_onlinePresence->onlineUsers());
                    updatePermissions();
                    emit membersChanged();
                    return;
                }

                const QString settingsPrefix = QStringLiteral("members:inviteSettings:");
                if (tag.startsWith(settingsPrefix)) {
                    if (tag.mid(settingsPrefix.size()).toInt() != m_inviteSettingsRequestId)
                        return;
                    setInviteSettingsLoading(false);
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    applyInviteSettings(result.toObject());
                    return;
                }

                const QString savePrefix = QStringLiteral("members:saveInviteSettings:");
                if (tag.startsWith(savePrefix)) {
                    if (tag.mid(savePrefix.size()) != m_currentServerId)
                        return;
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    applyInviteSettings(result.toObject());
                }
            });

    connect(m_client, &SupabaseClient::insertFinished, this,
            [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
                Q_UNUSED(rows)
                if (tag == QStringLiteral("members:createRole")) {
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    refreshRoles();
                } else if (tag == QStringLiteral("members:assignRole")) {
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    refreshMembers();
                    refreshInviteSettings();
                }
            });

    connect(m_client, &SupabaseClient::deleteFinished, this,
            [this](const QString &tag, bool ok, const QString &error) {
                if (tag == QStringLiteral("members:deleteRole")) {
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    refreshRoles();
                    refreshMembers();
                    refreshInviteSettings();
                } else if (tag == QStringLiteral("members:unassignRole")) {
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    refreshMembers();
                    refreshInviteSettings();
                } else if (tag == QStringLiteral("members:removeMember")) {
                    if (!ok) {
                        emit errorOccurred(error);
                        return;
                    }
                    refreshMembers();
                    refreshInviteSettings();
                }
            });
}

void ServerMembersController::watchServer(const QString &serverId)
{
    qInfo().noquote() << "ServerMembersController::watchServer" << "serverId=" << serverId;
    if (serverId == m_currentServerId)
        return;

    m_currentServerId = serverId;
    emit currentServerIdChanged();

    // Invalidate any in-flight request: its response belongs to the server we
    // just left and must not overwrite the new one.
    ++m_membersRequestId;
    ++m_rolesRequestId;
    ++m_inviteSettingsRequestId;
    setMembersLoading(false);
    setRolesLoading(false);
    setInviteSettingsLoading(false);

    m_roles.clear();
    emit rolesChanged();
    m_members.clear();
    resetInviteSettings();
    updatePermissions();
    emit membersChanged();

    if (serverId.isEmpty() || m_client->currentUserId().isEmpty())
        return;

    subscribeRealtime();
    refreshAll();
}

void ServerMembersController::subscribeRealtime()
{
    if (m_realtimeSubscribed)
        return;

    const QStringList tables{
        QStringLiteral("servers"),
        QStringLiteral("server_members"),
        QStringLiteral("server_member_roles"),
        QStringLiteral("server_roles"),
        QStringLiteral("server_invite_roles"),
        QStringLiteral("server_invite_members"),
    };

    QJsonArray changes;
    for (const QString &table : tables) {
        changes.append(QJsonObject{
            {"event", "*"},
            {"schema", "public"},
            {"table", table},
        });
    }

    m_realtime->joinTopic(kRealtimeTopic, changes);
    m_realtimeSubscribed = true;
}

void ServerMembersController::refreshAll()
{
    if (m_currentServerId.isEmpty())
        return;
    refreshMembers();
    refreshRoles();
    refreshInviteSettings();
}

void ServerMembersController::refreshMembers()
{
    if (m_currentServerId.isEmpty())
        return;

    const int requestId = ++m_membersRequestId;
    setMembersLoading(true);
    m_client->rpc(QStringLiteral("list_server_members"),
                  QJsonObject{{"p_server_id", m_currentServerId}},
                  QStringLiteral("members:list:%1").arg(requestId));
}

void ServerMembersController::refreshRoles()
{
    if (m_currentServerId.isEmpty())
        return;

    const int requestId = ++m_rolesRequestId;
    setRolesLoading(true);
    m_client->select(QStringLiteral("server_roles?server_id=eq.%1&select=*&order=position.desc,name.asc")
                         .arg(m_currentServerId),
                     QStringLiteral("members:roles:%1").arg(requestId));
}

void ServerMembersController::refreshInviteSettings()
{
    if (m_currentServerId.isEmpty())
        return;

    const int requestId = ++m_inviteSettingsRequestId;
    setInviteSettingsLoading(true);
    m_client->rpc(QStringLiteral("get_server_invite_settings"),
                  QJsonObject{{"p_server_id", m_currentServerId}},
                  QStringLiteral("members:inviteSettings:%1").arg(requestId));
}

void ServerMembersController::saveInviteSettings(const QString &policy,
                                                 const QVariantList &roleIds,
                                                 const QVariantList &memberIds)
{
    qInfo().noquote() << "ServerMembersController::saveInviteSettings"
                      << "policy=" << policy << "roles=" << roleIds.size()
                      << "members=" << memberIds.size();

    if (!m_canManageMembers) {
        emit errorOccurred(QStringLiteral("Voc\u00ea n\u00e3o tem permiss\u00e3o para configurar convites."));
        return;
    }
    if (m_currentServerId.isEmpty()
        || (policy != QLatin1String("everyone") && policy != QLatin1String("restricted"))) {
        return;
    }

    // Drop any in-flight read so its older snapshot cannot overwrite the
    // settings returned by this save.
    ++m_inviteSettingsRequestId;
    setInviteSettingsLoading(false);

    QJsonArray roles;
    for (const QVariant &roleId : roleIds)
        roles.append(roleId.toString());
    QJsonArray members;
    for (const QVariant &userId : memberIds)
        members.append(userId.toString());

    m_client->rpc(QStringLiteral("update_server_invite_settings"),
                  QJsonObject{
                      {"p_server_id", m_currentServerId},
                      {"p_policy", policy},
                      {"p_role_ids", roles},
                      {"p_member_ids", members},
                  },
                  QStringLiteral("members:saveInviteSettings:%1").arg(m_currentServerId));
}

void ServerMembersController::resetInviteSettings()
{
    const bool hadInvite = m_canInvite;
    m_canInvite = false;
    m_invitePolicy.clear();
    m_inviteRoleIds.clear();
    m_inviteMemberIds.clear();

    if (hadInvite)
        emit canInviteChanged();
    emit inviteSettingsChanged();
}

void ServerMembersController::applyInviteSettings(const QJsonObject &settings)
{
    const bool canInvite = settings.value(QStringLiteral("canInvite")).toBool(false);
    const QString policy = settings.value(QStringLiteral("policy")).toString(QStringLiteral("everyone"));

    QVariantList roleIds;
    for (const QJsonValue &value : settings.value(QStringLiteral("roles")).toArray())
        roleIds.append(value.toString());
    QVariantList memberIds;
    for (const QJsonValue &value : settings.value(QStringLiteral("members")).toArray())
        memberIds.append(value.toString());

    if (canInvite != m_canInvite) {
        m_canInvite = canInvite;
        emit canInviteChanged();
    }
    m_invitePolicy = policy;
    m_inviteRoleIds = roleIds;
    m_inviteMemberIds = memberIds;
    emit inviteSettingsChanged();
}

void ServerMembersController::setInviteSettingsLoading(bool loading)
{
    if (loading == m_inviteSettingsLoading)
        return;
    m_inviteSettingsLoading = loading;
    emit inviteSettingsLoadingChanged();
}

void ServerMembersController::createRole(const QString &name, const QString &color,
                                         const QVariantMap &permissions)
{
    qInfo().noquote() << "ServerMembersController::createRole" << "name=" << name;

    if (!m_canManageRoles) {
        emit errorOccurred(QStringLiteral("Voc\u00ea n\u00e3o tem permiss\u00e3o para gerenciar cargos."));
        return;
    }

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || m_currentServerId.isEmpty())
        return;

    m_client->insert(QStringLiteral("server_roles"),
                     QJsonObject{
                         {"server_id", m_currentServerId},
                         {"name", trimmed},
                         {"color", color.isEmpty() ? QStringLiteral("#5865F2") : color},
                         {"permissions", QJsonObject::fromVariantMap(permissions)},
                         {"position", m_roles.rowCount()},
                     },
                     QStringLiteral("members:createRole"));
}

void ServerMembersController::deleteRole(const QString &roleId)
{
    qInfo().noquote() << "ServerMembersController::deleteRole" << "roleId=" << roleId;

    if (!m_canManageRoles) {
        emit errorOccurred(QStringLiteral("Voc\u00ea n\u00e3o tem permiss\u00e3o para gerenciar cargos."));
        return;
    }
    if (roleId.isEmpty())
        return;

    m_client->remove(QStringLiteral("server_roles?id=eq.%1").arg(roleId),
                     QStringLiteral("members:deleteRole"));
}

void ServerMembersController::setRoleAssigned(const QString &userId, const QString &roleId,
                                              bool assigned)
{
    qInfo().noquote() << "ServerMembersController::setRoleAssigned"
                      << "userId=" << userId << "roleId=" << roleId << "assigned=" << assigned;

    if (!m_canManageRoles) {
        emit errorOccurred(QStringLiteral("Voc\u00ea n\u00e3o tem permiss\u00e3o para gerenciar cargos."));
        return;
    }
    if (userId.isEmpty() || roleId.isEmpty() || m_currentServerId.isEmpty())
        return;

    if (assigned) {
        m_client->insert(QStringLiteral("server_member_roles"),
                         QJsonObject{
                             {"server_id", m_currentServerId},
                             {"user_id", userId},
                             {"role_id", roleId},
                         },
                         QStringLiteral("members:assignRole"));
    } else {
        m_client->remove(QStringLiteral("server_member_roles?server_id=eq.%1&user_id=eq.%2&role_id=eq.%3")
                             .arg(m_currentServerId, userId, roleId),
                         QStringLiteral("members:unassignRole"));
    }
}

void ServerMembersController::removeMember(const QString &userId)
{
    qInfo().noquote() << "ServerMembersController::removeMember" << "userId=" << userId;

    if (!m_canManageMembers) {
        emit errorOccurred(QStringLiteral("Voc\u00ea n\u00e3o tem permiss\u00e3o para remover membros."));
        return;
    }
    if (userId.isEmpty() || m_currentServerId.isEmpty())
        return;

    m_client->remove(QStringLiteral("server_members?server_id=eq.%1&user_id=eq.%2")
                         .arg(m_currentServerId, userId),
                     QStringLiteral("members:removeMember"));
}

bool ServerMembersController::selfHasPermission(const QString &permission) const
{
    const ServerMemberEntry *self = m_members.memberById(m_client->currentUserId());
    if (!self)
        return false;

    if (self->memberRole == QLatin1String("owner")
        || self->memberRole == QLatin1String("admin")) {
        return true;
    }

    for (const QVariant &roleValue : self->roles) {
        const QVariantMap role = roleValue.toMap();
        const QVariantMap permissions = role.value(QStringLiteral("permissions")).toMap();
        if (permissions.value(permission).toBool())
            return true;
    }
    return false;
}

bool ServerMembersController::canGrantPermissions(const QVariantMap &permissions) const
{
    const ServerMemberEntry *self = m_members.memberById(m_client->currentUserId());
    if (!self)
        return false;

    if (self->memberRole == QLatin1String("owner")
        || self->memberRole == QLatin1String("admin")) {
        return true;
    }

    for (auto it = permissions.begin(); it != permissions.end(); ++it) {
        if (it.value().toBool() && !selfHasPermission(it.key()))
            return false;
    }
    return true;
}

void ServerMembersController::updatePermissions()
{
    const bool canManageRoles = selfHasPermission(QStringLiteral("manage_roles"));
    const bool canManageChannels = selfHasPermission(QStringLiteral("manage_channels"));
    const bool canManageMembers = selfHasPermission(QStringLiteral("manage_members"));

    if (canManageRoles != m_canManageRoles) {
        m_canManageRoles = canManageRoles;
        emit canManageRolesChanged();
    }
    if (canManageChannels != m_canManageChannels) {
        m_canManageChannels = canManageChannels;
        emit canManageChannelsChanged();
    }
    if (canManageMembers != m_canManageMembers) {
        m_canManageMembers = canManageMembers;
        emit canManageMembersChanged();
    }
}

void ServerMembersController::updateLoading()
{
    const bool loading = m_membersLoading || m_rolesLoading;
    if (loading == m_loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void ServerMembersController::setMembersLoading(bool loading)
{
    m_membersLoading = loading;
    updateLoading();
}

void ServerMembersController::setRolesLoading(bool loading)
{
    m_rolesLoading = loading;
    updateLoading();
}

QVariantMap ServerMembersController::memberToMap(const ServerMemberEntry &entry)
{
    QString highestRoleName;
    QString highestRoleColor;
    if (!entry.roles.isEmpty()) {
        const QVariantMap role = entry.roles.first().toMap();
        highestRoleName = role.value(QStringLiteral("name")).toString();
        highestRoleColor = role.value(QStringLiteral("color")).toString();
    }

    return QVariantMap{
        {"userId", entry.userId},
        {"username", entry.username},
        {"displayName", entry.displayName},
        {"avatarUrl", entry.avatarUrl},
        {"memberRole", entry.memberRole},
        {"roles", entry.roles},
        {"online", entry.online},
        {"highestRoleName", highestRoleName},
        {"highestRoleColor", highestRoleColor},
    };
}

QVariantList ServerMembersController::memberList(bool online) const
{
    QVariantList list;
    for (const ServerMemberEntry &entry : m_members.entries()) {
        if (entry.online == online)
            list.append(memberToMap(entry));
    }
    return list;
}
