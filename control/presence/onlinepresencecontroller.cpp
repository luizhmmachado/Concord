#include "onlinepresencecontroller.h"

#include <QJsonArray>

#include <QDebug>

OnlinePresenceController::OnlinePresenceController(SupabaseClient *client,
                                                   RealtimeClient *realtime,
                                                   QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    qInfo().noquote() << "OnlinePresenceController::OnlinePresenceController";

    connect(m_realtime, &RealtimeClient::presenceState, this,
            [this](const QString &topicName, const QJsonObject &state) {
                if (topicName == topic())
                    applyPresenceState(state);
            });
    connect(m_realtime, &RealtimeClient::presenceDiff, this,
            [this](const QString &topicName, const QJsonObject &diff) {
                if (topicName == topic())
                    applyPresenceDiff(diff);
            });
}

QString OnlinePresenceController::topic()
{
    return QStringLiteral("realtime:concord:online");
}

void OnlinePresenceController::setLocalUser(const QString &userId, const QString &displayName)
{
    qInfo().noquote() << "OnlinePresenceController::setLocalUser" << "userId=" << userId;

    if (userId.isEmpty()) {
        clear();
        return;
    }

    const bool userChanged = userId != m_localUserId;
    if (userChanged || !m_joined) {
        // Leaving the old topic removes the previous account's entry; the
        // presence state itself is re-created by trackPresence below.
        if (m_joined)
            m_realtime->leaveTopic(topic());
        m_localUserId = userId;
        m_realtime->joinTopic(topic(), QJsonArray(), userId);
        m_joined = true;
    }

    QJsonObject state{{"userId", userId}};
    if (!displayName.isEmpty())
        state.insert("displayName", displayName);

    if (userChanged || state != m_localState) {
        m_localState = state;
        m_realtime->trackPresence(topic(), state);
    }
}

void OnlinePresenceController::clear()
{
    if (!m_joined && m_localUserId.isEmpty())
        return;

    qInfo().noquote() << "OnlinePresenceController::clear";
    if (m_joined)
        m_realtime->leaveTopic(topic());

    m_joined = false;
    m_localUserId.clear();
    m_localState = QJsonObject();
    m_metas.clear();

    if (!m_onlineUsers.isEmpty()) {
        m_onlineUsers.clear();
        emit onlineUsersChanged();
    }
}

OnlinePresenceController::RefMap OnlinePresenceController::metasFrom(const QJsonValue &value)
{
    RefMap metas;

    const QJsonObject object = value.toObject();
    if (object.contains(QStringLiteral("metas"))) {
        int index = 0;
        for (const QJsonValue &entry : object.value(QStringLiteral("metas")).toArray()) {
            QJsonObject meta = entry.toObject();
            QString ref = meta.value(QStringLiteral("phx_ref")).toString();
            if (ref.isEmpty())
                ref = QString::number(index);
            metas.insert(ref, meta);
            ++index;
        }
    } else if (value.isArray()) {
        int index = 0;
        for (const QJsonValue &entry : value.toArray()) {
            metas.insert(QString::number(index), entry.toObject());
            ++index;
        }
    } else if (value.isObject()) {
        metas.insert(QStringLiteral("0"), object);
    }

    return metas;
}

void OnlinePresenceController::applyPresenceState(const QJsonObject &state)
{
    MetaMap metas;
    for (auto it = state.begin(); it != state.end(); ++it)
        metas.insert(it.key(), metasFrom(it.value()));

    m_metas = metas;
    rebuildOnlineUsers();
}

void OnlinePresenceController::applyPresenceDiff(const QJsonObject &diff)
{
    const QJsonObject joins = diff.value(QStringLiteral("joins")).toObject();
    for (auto it = joins.begin(); it != joins.end(); ++it) {
        const RefMap additions = metasFrom(it.value());
        for (auto addition = additions.begin(); addition != additions.end(); ++addition)
            m_metas[it.key()].insert(addition.key(), addition.value());
    }

    const QJsonObject leaves = diff.value(QStringLiteral("leaves")).toObject();
    for (auto it = leaves.begin(); it != leaves.end(); ++it) {
        auto keyIt = m_metas.find(it.key());
        if (keyIt == m_metas.end())
            continue;

        const RefMap removals = metasFrom(it.value());
        if (removals.isEmpty()) {
            m_metas.erase(keyIt);
        } else {
            for (auto removal = removals.begin(); removal != removals.end(); ++removal)
                keyIt.value().remove(removal.key());
            if (keyIt.value().isEmpty())
                m_metas.erase(keyIt);
        }
    }

    rebuildOnlineUsers();
}

void OnlinePresenceController::rebuildOnlineUsers()
{
    QVariantMap online;
    for (auto keyIt = m_metas.begin(); keyIt != m_metas.end(); ++keyIt) {
        QString displayName = keyIt.key();
        for (auto refIt = keyIt.value().begin(); refIt != keyIt.value().end(); ++refIt) {
            const QJsonObject &payload = refIt.value();
            const QString name = payload.value(QStringLiteral("displayName")).toString();
            if (!name.isEmpty()) {
                displayName = name;
                break;
            }
        }
        online.insert(keyIt.key(), displayName);
    }

    if (online == m_onlineUsers)
        return;

    m_onlineUsers = online;
    emit onlineUsersChanged();
}
