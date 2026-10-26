#include "serverlistmodel.h"

#include <QDebug>
ServerListModel::ServerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "ServerListModel::ServerListModel";
}

int ServerListModel::rowCount(const QModelIndex &parent) const
{
    qInfo().noquote() << "ServerListModel::rowCount";
    if (parent.isValid())
        return 0;
    return m_servers.size();
}

QVariant ServerListModel::data(const QModelIndex &index, int role) const
{
    qInfo().noquote() << "ServerListModel::data" << "index=" << index << "role=" << role;
    if (!index.isValid() || index.row() >= m_servers.size())
        return {};

    const ServerEntry &entry = m_servers.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case NameRole: return entry.name;
    case IconUrlRole: return entry.iconUrl;
    case OwnerIdRole: return entry.ownerId;
    default: return {};
    }
}

QHash<int, QByteArray> ServerListModel::roleNames() const
{
    qInfo().noquote() << "ServerListModel::roleNames";
    return {
        {IdRole, "serverId"},
        {NameRole, "name"},
        {IconUrlRole, "iconUrl"},
        {OwnerIdRole, "ownerId"},
    };
}

void ServerListModel::setServers(const QJsonArray &rows)
{
    qInfo().noquote() << "ServerListModel::setServers" << "rows=" << rows;
    beginResetModel();
    m_servers.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        m_servers.append(ServerEntry{
            obj.value("id").toString(),
            obj.value("name").toString(),
            obj.value("icon_url").toString(),
            obj.value("owner_id").toString(),
        });
    }
    endResetModel();
}

void ServerListModel::upsertServer(const QJsonObject &row)
{
    qInfo().noquote() << "ServerListModel::upsertServer" << "row=" << row;
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
    });
    endInsertRows();
}

QString ServerListModel::serverIdAt(int row) const
{
    qInfo().noquote() << "ServerListModel::serverIdAt" << "row=" << row;
    if (row < 0 || row >= m_servers.size())
        return {};
    return m_servers.at(row).id;
}

const ServerEntry *ServerListModel::serverById(const QString &serverId) const
{
    for (const ServerEntry &entry : m_servers) {
        if (entry.id == serverId)
            return &entry;
    }
    return nullptr;
}
