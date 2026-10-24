#include "servermembermodel.h"

#include <QDebug>

ServerMemberModel::ServerMemberModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "ServerMemberModel::ServerMemberModel";
}

int ServerMemberModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_members.size();
}

QVariant ServerMemberModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_members.size())
        return {};

    const ServerMemberEntry &entry = m_members.at(index.row());
    switch (role) {
    case UserIdRole: return entry.userId;
    case UsernameRole: return entry.username;
    case DisplayNameRole: return entry.displayName;
    case AvatarUrlRole: return entry.avatarUrl;
    case MemberRoleRole: return entry.memberRole;
    case RolesRole: return entry.roles;
    case OnlineRole: return entry.online;
    case HighestRoleNameRole: {
        if (entry.roles.isEmpty())
            return QString();
        return entry.roles.first().toMap().value(QStringLiteral("name")).toString();
    }
    case HighestRoleColorRole: {
        if (entry.roles.isEmpty())
            return QString();
        return entry.roles.first().toMap().value(QStringLiteral("color")).toString();
    }
    default: return {};
    }
}

QHash<int, QByteArray> ServerMemberModel::roleNames() const
{
    return {
        {UserIdRole, "userId"},
        {UsernameRole, "username"},
        {DisplayNameRole, "displayName"},
        {AvatarUrlRole, "avatarUrl"},
        {MemberRoleRole, "memberRole"},
        {RolesRole, "roles"},
        {OnlineRole, "online"},
        {HighestRoleNameRole, "highestRoleName"},
        {HighestRoleColorRole, "highestRoleColor"},
    };
}

void ServerMemberModel::setMembers(const QJsonArray &rows)
{
    qInfo().noquote() << "ServerMemberModel::setMembers" << "rows=" << rows.size();

    beginResetModel();
    m_members.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("user_id")).toString().isEmpty())
            continue;

        QVariantList roles;
        for (const QJsonValue &roleValue : obj.value(QStringLiteral("roles")).toArray()) {
            const QJsonObject role = roleValue.toObject();
            roles.append(QVariantMap{
                {"id", role.value(QStringLiteral("id")).toString()},
                {"name", role.value(QStringLiteral("name")).toString()},
                {"color", role.value(QStringLiteral("color")).toString()},
                {"permissions", role.value(QStringLiteral("permissions")).toObject().toVariantMap()},
                {"position", role.value(QStringLiteral("position")).toInt()},
            });
        }

        const QString userId = obj.value(QStringLiteral("user_id")).toString();
        m_members.append(ServerMemberEntry{
            userId,
            obj.value(QStringLiteral("username")).toString(),
            obj.value(QStringLiteral("display_name")).toString(),
            obj.value(QStringLiteral("avatar_url")).toString(),
            obj.value(QStringLiteral("member_role")).toString(),
            roles,
            m_onlineUsers.contains(userId),
        });
    }
    endResetModel();
}

void ServerMemberModel::setOnlineUsers(const QVariantMap &onlineUsers)
{
    m_onlineUsers = onlineUsers;

    bool changed = false;
    for (int i = 0; i < m_members.size(); ++i) {
        const bool online = m_onlineUsers.contains(m_members.at(i).userId);
        if (m_members[i].online == online)
            continue;
        m_members[i].online = online;
        changed = true;
    }

    if (changed && !m_members.isEmpty())
        emit dataChanged(index(0), index(m_members.size() - 1), {OnlineRole});
}

void ServerMemberModel::clear()
{
    beginResetModel();
    m_members.clear();
    endResetModel();
}

const ServerMemberEntry *ServerMemberModel::memberById(const QString &userId) const
{
    for (const ServerMemberEntry &entry : m_members) {
        if (entry.userId == userId)
            return &entry;
    }
    return nullptr;
}
