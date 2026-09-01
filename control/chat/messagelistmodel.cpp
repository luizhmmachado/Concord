#include "messagelistmodel.h"

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

MessageEntry MessageListModel::fromJson(const QJsonObject &obj)
{
    const QJsonObject author = obj.value("profiles").toObject();
    return MessageEntry{
        obj.value("id").toString(),
        obj.value("author_id").toString(),
        author.value("display_name").toString(author.value("username").toString()),
        obj.value("content").toString(),
        obj.value("created_at").toString(),
    };
}

int MessageListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_messages.size();
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return {};

    const MessageEntry &entry = m_messages.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case AuthorIdRole: return entry.authorId;
    case AuthorNameRole: return entry.authorName;
    case ContentRole: return entry.content;
    case CreatedAtRole: return entry.createdAt;
    default: return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {IdRole, "messageId"},
        {AuthorIdRole, "authorId"},
        {AuthorNameRole, "authorName"},
        {ContentRole, "content"},
        {CreatedAtRole, "createdAt"},
    };
}

void MessageListModel::setMessages(const QJsonArray &rows)
{
    beginResetModel();
    m_messages.clear();
    for (const QJsonValue &value : rows)
        m_messages.append(fromJson(value.toObject()));
    endResetModel();
}

void MessageListModel::appendMessage(const QJsonObject &row)
{
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(fromJson(row));
    endInsertRows();
}

void MessageListModel::clear()
{
    beginResetModel();
    m_messages.clear();
    endResetModel();
}
