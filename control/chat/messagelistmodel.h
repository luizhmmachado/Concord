#ifndef MESSAGELISTMODEL_H
#define MESSAGELISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>

struct MessageEntry {
    QString id;
    QString authorId;
    QString authorName;
    QString content;
    QString imageUrl;
    QString createdAt;
    QString timeLabel;
    // Non-empty only on the first message of a day, e.g. "24 de junho de 2026".
    QString daySeparator;
    // True when it continues the previous author's block (no avatar/header).
    bool grouped = false;
};

class MessageListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        AuthorIdRole,
        AuthorNameRole,
        ContentRole,
        ImageUrlRole,
        CreatedAtRole,
        TimeLabelRole,
        DaySeparatorRole,
        GroupedRole,
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QJsonArray &rows);
    void appendMessage(const QJsonObject &row);
    void clear();

    // Used when a realtime payload arrives without the joined profile row.
    void setAuthorNameFallback(const QString &authorId, const QString &name);

private:
    MessageEntry fromJson(const QJsonObject &obj) const;
    void applyDaySeparator(MessageEntry &entry, const MessageEntry *previous) const;
    void applyGrouping(MessageEntry &entry, const MessageEntry *previous) const;
    int indexOfId(const QString &id) const;

    QHash<QString, QString> m_authorNameFallback;
    QVector<MessageEntry> m_messages;
};

#endif // MESSAGELISTMODEL_H
