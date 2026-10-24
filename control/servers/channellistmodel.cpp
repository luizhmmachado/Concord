#include "channellistmodel.h"

#include <algorithm>

#include <QDebug>
ChannelListModel::ChannelListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "ChannelListModel::ChannelListModel";
}

int ChannelListModel::rowCount(const QModelIndex &parent) const
{
    qInfo().noquote() << "ChannelListModel::rowCount";
    if (parent.isValid())
        return 0;
    return m_channels.size();
}

QVariant ChannelListModel::data(const QModelIndex &index, int role) const
{
    qInfo().noquote() << "ChannelListModel::data" << "index=" << index << "role=" << role;
    if (!index.isValid() || index.row() >= m_channels.size())
        return {};

    const ChannelEntry &entry = m_channels.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case ServerIdRole: return entry.serverId;
    case NameRole: return entry.name;
    case TypeRole: return entry.type;
    case PositionRole: return entry.position;
    default: return {};
    }
}

QHash<int, QByteArray> ChannelListModel::roleNames() const
{
    qInfo().noquote() << "ChannelListModel::roleNames";
    return {
        {IdRole, "channelId"},
        {ServerIdRole, "serverId"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {PositionRole, "position"},
    };
}

void ChannelListModel::setChannels(const QJsonArray &rows)
{
    qInfo().noquote() << "ChannelListModel::setChannels" << "rows=" << rows;
    beginResetModel();
    m_channels.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        m_channels.append(ChannelEntry{
            obj.value("id").toString(),
            obj.value("server_id").toString(),
            obj.value("name").toString(),
            obj.value("type").toString(),
            obj.value("position").toInt(),
        });
    }
    std::sort(m_channels.begin(), m_channels.end(), [](const ChannelEntry &a, const ChannelEntry &b) {
        return a.position < b.position;
    });
    endResetModel();
}

void ChannelListModel::addChannel(const QJsonObject &row)
{
    qInfo().noquote() << "ChannelListModel::addChannel" << "row=" << row;
    beginInsertRows(QModelIndex(), m_channels.size(), m_channels.size());
    m_channels.append(ChannelEntry{
        row.value("id").toString(),
        row.value("server_id").toString(),
        row.value("name").toString(),
        row.value("type").toString(),
        row.value("position").toInt(),
    });
    endInsertRows();
}

QString ChannelListModel::channelIdAt(int row) const
{
    qInfo().noquote() << "ChannelListModel::channelIdAt" << "row=" << row;
    if (row < 0 || row >= m_channels.size())
        return {};
    return m_channels.at(row).id;
}

QString ChannelListModel::channelTypeAt(int row) const
{
    qInfo().noquote() << "ChannelListModel::channelTypeAt" << "row=" << row;
    if (row < 0 || row >= m_channels.size())
        return {};
    return m_channels.at(row).type;
}
