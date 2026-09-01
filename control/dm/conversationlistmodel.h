#ifndef CONVERSATIONLISTMODEL_H
#define CONVERSATIONLISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QVector>

struct ConversationEntry {
    QString conversationId;
    QString peerId;
    QString peerUsername;
    QString peerName;
    QString lastMessage;
    QString lastMessageAt;
};

// One row per 1:1 conversation the current user takes part in.
class ConversationListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        ConversationIdRole = Qt::UserRole + 1,
        PeerIdRole,
        PeerUsernameRole,
        PeerNameRole,
        LastMessageRole,
        LastMessageAtRole,
    };

    explicit ConversationListModel(QObject *parent = nullptr);

    int count() const { return m_conversations.size(); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setConversations(const QJsonArray &rows);
    void setLastMessage(const QString &conversationId, const QString &content, const QString &createdAt);
    void clear();

    QString peerNameFor(const QString &conversationId) const;
    QString peerIdFor(const QString &conversationId) const;

signals:
    void countChanged();

private:
    QVector<ConversationEntry> m_conversations;
};

#endif // CONVERSATIONLISTMODEL_H
