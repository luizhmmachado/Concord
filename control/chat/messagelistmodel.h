#ifndef MESSAGELISTMODEL_H
#define MESSAGELISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

struct MessageEntry {
    QString id;
    QString authorId;
    QString authorName;
    QString content;
    QString createdAt;
};

class MessageListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, AuthorIdRole, AuthorNameRole, ContentRole, CreatedAtRole };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QJsonArray &rows);
    void appendMessage(const QJsonObject &row);
    void clear();

private:
    static MessageEntry fromJson(const QJsonObject &obj);

    QVector<MessageEntry> m_messages;
};

#endif // MESSAGELISTMODEL_H
