#include "serverrolemodel.h"

#include <QDebug>

ServerRoleModel::ServerRoleModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "ServerRoleModel::ServerRoleModel";
}

int ServerRoleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_roles.size();
}

QVariant ServerRoleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_roles.size())
        return {};

    const ServerRoleEntry &entry = m_roles.at(index.row());
    switch (role) {
    case RoleIdRole: return entry.id;
    case NameRole: return entry.name;
    case ColorRole: return entry.color;
    case PermissionsRole: return entry.permissions;
    case PositionRole: return entry.position;
    default: return {};
    }
}

QHash<int, QByteArray> ServerRoleModel::roleNames() const
{
    return {
        {RoleIdRole, "roleId"},
        {NameRole, "name"},
        {ColorRole, "color"},
        {PermissionsRole, "permissions"},
        {PositionRole, "position"},
    };
}

void ServerRoleModel::setRoles(const QJsonArray &rows)
{
    qInfo().noquote() << "ServerRoleModel::setRoles" << "rows=" << rows.size();

    beginResetModel();
    m_roles.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        if (obj.value(QStringLiteral("id")).toString().isEmpty())
            continue;
        m_roles.append(ServerRoleEntry{
            obj.value(QStringLiteral("id")).toString(),
            obj.value(QStringLiteral("name")).toString(),
            obj.value(QStringLiteral("color")).toString(),
            obj.value(QStringLiteral("permissions")).toObject().toVariantMap(),
            obj.value(QStringLiteral("position")).toInt(),
        });
    }
    endResetModel();
}

void ServerRoleModel::clear()
{
    beginResetModel();
    m_roles.clear();
    endResetModel();
}

const ServerRoleEntry *ServerRoleModel::roleById(const QString &roleId) const
{
    for (const ServerRoleEntry &entry : m_roles) {
        if (entry.id == roleId)
            return &entry;
    }
    return nullptr;
}
