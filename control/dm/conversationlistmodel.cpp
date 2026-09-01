#include "conversationlistmodel.h"

#include <QJsonObject>

ConversationListModel::ConversationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_conversations.size();
}

QVariant ConversationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_conversations.size())
        return {};

    const ConversationEntry &entry = m_conversations.at(index.row());
    switch (role) {
    case ConversationIdRole: return entry.conversationId;
    case PeerIdRole: return entry.peerId;
    case PeerUsernameRole: return entry.peerUsername;
    case PeerNameRole: return entry.peerName;
    case LastMessageRole: return entry.lastMessage;
    case LastMessageAtRole: return entry.lastMessageAt;
    default: return {};
    }
}

QHash<int, QByteArray> ConversationListModel::roleNames() const
{
    return {
        {ConversationIdRole, "conversationId"},
        {PeerIdRole, "peerId"},
        {PeerUsernameRole, "peerUsername"},
        {PeerNameRole, "peerName"},
        {LastMessageRole, "lastMessage"},
        {LastMessageAtRole, "lastMessageAt"},
    };
}

void ConversationListModel::setConversations(const QJsonArray &rows)
{
    beginResetModel();
    m_conversations.clear();
    for (const QJsonValue &value : rows) {
        const QJsonObject obj = value.toObject();
        ConversationEntry entry;
        entry.conversationId = obj.value("conversation_id").toString();
        entry.peerId = obj.value("peer_id").toString();
        entry.peerUsername = obj.value("peer_username").toString();
        entry.peerName = obj.value("peer_display_name").toString(entry.peerUsername);
        entry.lastMessage = obj.value("last_message").toString();
        entry.lastMessageAt = obj.value("last_message_at").toString();
        m_conversations.append(entry);
    }
    endResetModel();
    emit countChanged();
}

void ConversationListModel::setLastMessage(const QString &conversationId, const QString &content, const QString &createdAt)
{
    for (int i = 0; i < m_conversations.size(); ++i) {
        if (m_conversations.at(i).conversationId != conversationId)
            continue;
        m_conversations[i].lastMessage = content;
        m_conversations[i].lastMessageAt = createdAt;
        emit dataChanged(index(i), index(i), {LastMessageRole, LastMessageAtRole});
        return;
    }
}

QString ConversationListModel::peerNameFor(const QString &conversationId) const
{
    for (const ConversationEntry &entry : m_conversations) {
        if (entry.conversationId == conversationId)
            return entry.peerName;
    }
    return {};
}

QString ConversationListModel::peerIdFor(const QString &conversationId) const
{
    for (const ConversationEntry &entry : m_conversations) {
        if (entry.conversationId == conversationId)
            return entry.peerId;
    }
    return {};
}

void ConversationListModel::clear()
{
    beginResetModel();
    m_conversations.clear();
    endResetModel();
    emit countChanged();
}
