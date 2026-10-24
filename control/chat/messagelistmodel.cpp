#include "messagelistmodel.h"

#include <QDateTime>
#include <QLocale>

#include <QDebug>
MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "MessageListModel::MessageListModel";
}

MessageEntry MessageListModel::fromJson(const QJsonObject &obj) const
{
    qInfo().noquote() << "MessageListModel::fromJson" << "obj=" << obj;
    const QJsonObject author = obj.value("profiles").toObject();
    const QString authorId = obj.value("author_id").toString();

    QString authorName = author.value("display_name").toString(author.value("username").toString());
    if (authorName.isEmpty())
        authorName = m_authorNameFallback.value(authorId);

    const QString createdAt = obj.value("created_at").toString();
    const QDateTime timestamp = QDateTime::fromString(createdAt, Qt::ISODateWithMs).toLocalTime();

    MessageEntry entry;
    entry.id = obj.value("id").toString();
    entry.authorId = authorId;
    entry.authorName = authorName;
    entry.content = obj.value("content").toString();
    entry.imageUrl = obj.value("image_url").toString();
    entry.createdAt = createdAt;
    entry.timeLabel = timestamp.isValid() ? timestamp.toString(QStringLiteral("HH:mm")) : QString();
    return entry;
}

void MessageListModel::applyDaySeparator(MessageEntry &entry, const MessageEntry *previous) const
{
    qInfo().noquote() << "MessageListModel::applyDaySeparator" << "entryId=" << entry.id;
    const QDate day = QDateTime::fromString(entry.createdAt, Qt::ISODateWithMs).toLocalTime().date();
    if (!day.isValid()) {
        entry.daySeparator.clear();
        return;
    }

    if (previous) {
        const QDate previousDay = QDateTime::fromString(previous->createdAt, Qt::ISODateWithMs).toLocalTime().date();
        if (previousDay == day) {
            entry.daySeparator.clear();
            return;
        }
    }

    const QDate today = QDate::currentDate();
    if (day == today)
        entry.daySeparator = tr("Hoje");
    else if (day == today.addDays(-1))
        entry.daySeparator = tr("Ontem");
    else
        entry.daySeparator = QLocale().toString(day, QStringLiteral("d 'de' MMMM 'de' yyyy"));
}

void MessageListModel::applyGrouping(MessageEntry &entry, const MessageEntry *previous) const
{
    qInfo().noquote() << "MessageListModel::applyGrouping" << "entryId=" << entry.id;
    if (!previous || !entry.daySeparator.isEmpty() || previous->authorId != entry.authorId) {
        entry.grouped = false;
        return;
    }

    const QDateTime current = QDateTime::fromString(entry.createdAt, Qt::ISODateWithMs);
    const QDateTime earlier = QDateTime::fromString(previous->createdAt, Qt::ISODateWithMs);
    // Discord starts a fresh block once the author pauses for a few minutes.
    entry.grouped = current.isValid() && earlier.isValid() && earlier.secsTo(current) <= 7 * 60;
}

int MessageListModel::indexOfId(const QString &id) const
{
    qInfo().noquote() << "MessageListModel::indexOfId" << "id=" << id;
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages.at(i).id == id)
            return i;
    }
    return -1;
}

void MessageListModel::setAuthorNameFallback(const QString &authorId, const QString &name)
{
    qInfo().noquote() << "MessageListModel::setAuthorNameFallback" << "authorId=" << authorId << "name=" << name;
    if (authorId.isEmpty() || name.isEmpty())
        return;
    m_authorNameFallback.insert(authorId, name);
}

int MessageListModel::rowCount(const QModelIndex &parent) const
{
    qInfo().noquote() << "MessageListModel::rowCount";
    if (parent.isValid())
        return 0;
    return m_messages.size();
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    qInfo().noquote() << "MessageListModel::data" << "index=" << index << "role=" << role;
    if (!index.isValid() || index.row() >= m_messages.size())
        return {};

    const MessageEntry &entry = m_messages.at(index.row());
    switch (role) {
    case IdRole: return entry.id;
    case AuthorIdRole: return entry.authorId;
    case AuthorNameRole: return entry.authorName;
    case ContentRole: return entry.content;
    case ImageUrlRole: return entry.imageUrl;
    case CreatedAtRole: return entry.createdAt;
    case TimeLabelRole: return entry.timeLabel;
    case DaySeparatorRole: return entry.daySeparator;
    case GroupedRole: return entry.grouped;
    default: return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    qInfo().noquote() << "MessageListModel::roleNames";
    return {
        {IdRole, "messageId"},
        {AuthorIdRole, "authorId"},
        {AuthorNameRole, "authorName"},
        {ContentRole, "content"},
        {ImageUrlRole, "imageUrl"},
        {CreatedAtRole, "createdAt"},
        {TimeLabelRole, "timeLabel"},
        {DaySeparatorRole, "daySeparator"},
        {GroupedRole, "grouped"},
    };
}

void MessageListModel::setMessages(const QJsonArray &rows)
{
    qInfo().noquote() << "MessageListModel::setMessages" << "rows=" << rows;
    beginResetModel();
    m_messages.clear();
    for (const QJsonValue &value : rows) {
        MessageEntry entry = fromJson(value.toObject());
        const MessageEntry *previous = m_messages.isEmpty() ? nullptr : &m_messages.last();
        applyDaySeparator(entry, previous);
        applyGrouping(entry, previous);
        m_messages.append(entry);
    }
    endResetModel();
}

void MessageListModel::appendMessage(const QJsonObject &row)
{
    qInfo().noquote() << "MessageListModel::appendMessage" << "row=" << row;
    MessageEntry entry = fromJson(row);

    // The sender receives its own message twice (the insert response and the
    // realtime broadcast); keep whichever carries the author name.
    const int existing = indexOfId(entry.id);
    if (existing >= 0) {
        if (entry.authorName.isEmpty() || entry.authorName == m_messages.at(existing).authorName)
            return;
        entry.daySeparator = m_messages.at(existing).daySeparator;
        entry.grouped = m_messages.at(existing).grouped;
        m_messages[existing] = entry;
        emit dataChanged(index(existing), index(existing));
        return;
    }

    const MessageEntry *previous = m_messages.isEmpty() ? nullptr : &m_messages.last();
    applyDaySeparator(entry, previous);
    applyGrouping(entry, previous);

    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(entry);
    endInsertRows();
}

void MessageListModel::clear()
{
    qInfo().noquote() << "MessageListModel::clear";
    beginResetModel();
    m_messages.clear();
    endResetModel();
}
