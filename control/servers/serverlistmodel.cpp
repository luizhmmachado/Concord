#include "serverlistmodel.h"

ServerListModel::ServerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ServerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_servers.size();
}

QVariant ServerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_servers.size())
        return {};

    const ServerEntry &entry = m_servers.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case NameRole: return entry.name;
    case IconUrlRole: return entry.iconUrl;
    case OwnerIdRole: return entry.ownerId;
    case InviteCodeRole: return entry.inviteCode;
    default: return {};
    }
}

QHash<int, QByteArray> ServerListModel::roleNames() const
{
    return {
        {IdRole, "serverId"},
        {NameRole, "name"},
        {IconUrlRole, "iconUrl"},
        {OwnerIdRole, "ownerId"},
        {InviteCodeRole, "inviteCode"},
    };
}

void ServerListModel::setServers(const QJsonArray &rows)
{
    beginResetModel();
    m_servers.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        m_servers.append(ServerEntry{
            obj.value("id").toString(),
            obj.value("name").toString(),
            obj.value("icon_url").toString(),
            obj.value("owner_id").toString(),
            obj.value("invite_code").toString(),
        });
    }
    endResetModel();
}

void ServerListModel::upsertServer(const QJsonObject &row)
{
    const QString id = row.value("id").toString();
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers.at(i).id == id) {
            m_servers[i].name = row.value("name").toString();
            m_servers[i].iconUrl = row.value("icon_url").toString();
            emit dataChanged(index(i), index(i));
            return;
        }
    }
    beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
    m_servers.append(ServerEntry{
        id,
        row.value("name").toString(),
        row.value("icon_url").toString(),
        row.value("owner_id").toString(),
        row.value("invite_code").toString(),
    });
    endInsertRows();
}

QString ServerListModel::serverIdAt(int row) const
{
    if (row < 0 || row >= m_servers.size())
        return {};
    return m_servers.at(row).id;
}
