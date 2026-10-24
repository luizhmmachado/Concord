#include "callpresencecontroller.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QVariantList>

#include <QDebug>

CallPresenceController::CallPresenceController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    qInfo().noquote() << "CallPresenceController::CallPresenceController" << "client=" << client << "realtime=" << realtime;

    connect(m_realtime, &RealtimeClient::presenceState, this, [this](const QString &topic, const QJsonObject &state) {
        applyPresenceState(topic, state);
        // Presence only carries the state from the last track, which happens on
        // join; mute/camera/screen changes are broadcasts. Ask whoever is in a
        // call for their current flags so a late subscriber is not left with
        // stale icons. A request is a broadcast, so it costs no presence quota.
        if (!state.isEmpty() && (topic == m_watchTopic || topic == m_publishTopic))
            m_realtime->broadcast(topic, QStringLiteral("voice_state_request"), QJsonObject{});
    });
    connect(m_realtime, &RealtimeClient::presenceDiff, this, [this](const QString &topic, const QJsonObject &diff) {
        applyPresenceDiff(topic, diff);
    });
    connect(m_realtime, &RealtimeClient::broadcastReceived, this, [this](const QString &topic, const QString &event, const QJsonObject &payload) {
        if (event == QStringLiteral("voice_state_request")) {
            if (topic == m_publishTopic && !m_localState.isEmpty())
                m_realtime->broadcast(topic, QStringLiteral("voice_state"), m_localState);
            return;
        }
        if (event != QStringLiteral("voice_state"))
            return;
        const QString userId = payload.value(QStringLiteral("userId")).toString();
        if (userId.isEmpty())
            return;
        const QString channelId = payload.value(QStringLiteral("channelId")).toString();
        const QString cacheKey = channelId.isEmpty() ? userId
                                                     : channelId + QLatin1Char(':') + userId;
        // Cache for every joined topic; only the watched one is rendered.
        m_liveFlagsByTopic[topic].insert(cacheKey, payload);
        if (topic == m_watchTopic)
            rebuildPresence();
    });
}

QString CallPresenceController::topicForServer(const QString &serverId)
{
    return QStringLiteral("realtime:presence:%1").arg(serverId);
}

void CallPresenceController::watchServer(const QString &serverId)
{
    qInfo().noquote() << "CallPresenceController::watchServer" << "serverId=" << serverId;
    if (serverId.isEmpty() || serverId == m_watchServerId || m_client->currentUserId().isEmpty())
        return;

    m_watchServerId = serverId;
    m_watchTopic = topicForServer(serverId);

    // Subscribing is idempotent; joining with our key enables presence events.
    // We keep previously watched topics joined (like Discord keeps guild
    // subscriptions), so their snapshots stay warm and switching back is
    // instant.
    m_realtime->joinTopic(m_watchTopic, QJsonArray(), m_client->currentUserId());

    // The topic may already be joined from an earlier visit, so no fresh
    // presence_state will arrive: ask the people in calls for their live flags.
    if (!m_metasByTopic.value(m_watchTopic).isEmpty())
        m_realtime->broadcast(m_watchTopic, QStringLiteral("voice_state_request"), QJsonObject{});

    rebuildPresence();
}

void CallPresenceController::setLocalCallState(const QString &serverId, const QString &channelId,
                                               const QString &displayName, bool muted,
                                               bool cameraOn, bool screenSharing)
{
    if (serverId.isEmpty() || channelId.isEmpty()) {
        clearLocalCallState();
        return;
    }

    const QString userId = m_client->currentUserId();
    if (userId.isEmpty())
        return;

    const QString topic = topicForServer(serverId);
    const QJsonObject state{
        {"userId", userId},
        {"channelId", channelId},
        {"displayName", displayName},
        {"muted", muted},
        {"cameraOn", cameraOn},
        {"screenSharing", screenSharing},
    };

    const bool membershipChanged = topic != m_publishTopic || channelId != m_localChannelId;
    if (topic != m_publishTopic) {
        if (!m_publishTopic.isEmpty())
            m_realtime->untrackPresence(m_publishTopic);
        m_publishTopic = topic;
        m_realtime->joinTopic(topic, QJsonArray(), userId);
    }

    if (membershipChanged) {
        m_realtime->trackPresence(topic, state);
    } else if (state != m_localState) {
        // Track/untrack count against the 5-per-30s presence quota; flag
        // changes are frequent, so they go out as a cheap broadcast instead.
        m_realtime->broadcast(topic, QStringLiteral("voice_state"), state);
    }

    m_localChannelId = channelId;
    m_localState = state;
    rebuildPresence();
}

void CallPresenceController::clearLocalCallState()
{
    if (!m_publishTopic.isEmpty())
        m_realtime->untrackPresence(m_publishTopic);

    m_publishTopic.clear();
    m_localChannelId.clear();
    m_localState = QJsonObject();
    rebuildPresence();
}

CallPresenceController::RefMap CallPresenceController::metasFrom(const QJsonValue &value)
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
        // Some protocol versions send the payload without the "metas" wrapper.
        metas.insert(QStringLiteral("0"), object);
    }

    return metas;
}

void CallPresenceController::applyPresenceState(const QString &topic, const QJsonObject &state)
{
    MetaMap metas;
    for (auto it = state.begin(); it != state.end(); ++it)
        metas.insert(it.key(), metasFrom(it.value()));

    m_metasByTopic.insert(topic, metas);
    // A full snapshot is authoritative; any broadcast-only flags we cached for
    // this topic may predate a reconnect.
    m_liveFlagsByTopic.remove(topic);

    if (topic == m_watchTopic)
        rebuildPresence();
}

void CallPresenceController::applyPresenceDiff(const QString &topic, const QJsonObject &diff)
{
    MetaMap &metas = m_metasByTopic[topic];

    const QJsonObject joins = diff.value(QStringLiteral("joins")).toObject();
    for (auto it = joins.begin(); it != joins.end(); ++it) {
        const RefMap additions = metasFrom(it.value());
        for (auto addition = additions.begin(); addition != additions.end(); ++addition)
            metas[it.key()].insert(addition.key(), addition.value());
    }

    const QJsonObject leaves = diff.value(QStringLiteral("leaves")).toObject();
    for (auto it = leaves.begin(); it != leaves.end(); ++it) {
        auto keyIt = metas.find(it.key());
        if (keyIt == metas.end())
            continue;

        const RefMap removals = metasFrom(it.value());
        if (removals.isEmpty()) {
            metas.erase(keyIt);
        } else {
            for (auto removal = removals.begin(); removal != removals.end(); ++removal)
                keyIt.value().remove(removal.key());
            if (keyIt.value().isEmpty())
                metas.erase(keyIt);
        }

        // A user who left the topic must not keep flag overrides from the
        // previous session; otherwise rejoining would show stale mute/camera.
        if (!metas.contains(it.key())) {
            auto &flags = m_liveFlagsByTopic[topic];
            for (auto flag = flags.begin(); flag != flags.end();) {
                if (flag.key() == it.key()
                    || flag.key().endsWith(QLatin1Char(':') + it.key())) {
                    flag = flags.erase(flag);
                } else {
                    ++flag;
                }
            }
        }
    }

    if (topic == m_watchTopic)
        rebuildPresence();
}

void CallPresenceController::rebuildPresence()
{
    const MetaMap metas = m_metasByTopic.value(m_watchTopic);
    const QString localUserId = m_client->currentUserId();
    const bool showLocal = !m_publishTopic.isEmpty() && m_publishTopic == m_watchTopic
                        && !m_localChannelId.isEmpty();

    QVariantMap presence;
    QSet<QString> seen;

    const auto appendEntry = [&](const QString &key, const QJsonObject &payload,
                                 const QHash<QString, QJsonObject> &liveFlags) {
        QString channelId = payload.value(QStringLiteral("channelId")).toString();
        if (channelId.isEmpty())
            channelId = payload.value(QStringLiteral("channel_id")).toString();
        if (channelId.isEmpty())
            return;

        QString userId = payload.value(QStringLiteral("userId")).toString();
        if (userId.isEmpty())
            userId = key;

        const QString dedupeKey = channelId + QLatin1Char(':') + userId;
        if (seen.contains(dedupeKey))
            return;
        seen.insert(dedupeKey);

        const QJsonObject live = liveFlags.value(dedupeKey);

        QString displayName = payload.value(QStringLiteral("displayName")).toString();
        if (live.contains(QStringLiteral("displayName")))
            displayName = live.value(QStringLiteral("displayName")).toString();

        QVariantList entries = presence.value(channelId).toList();
        entries.append(QVariantMap{
            {"userId", userId},
            {"displayName", displayName},
            {"muted", live.contains(QStringLiteral("muted")) ? live.value(QStringLiteral("muted")).toBool()
                                                             : payload.value(QStringLiteral("muted")).toBool()},
            {"cameraOn", live.contains(QStringLiteral("cameraOn")) ? live.value(QStringLiteral("cameraOn")).toBool()
                                                                 : payload.value(QStringLiteral("cameraOn")).toBool()},
            {"screenSharing", live.contains(QStringLiteral("screenSharing")) ? live.value(QStringLiteral("screenSharing")).toBool()
                                                                            : payload.value(QStringLiteral("screenSharing")).toBool()},
        });
        presence.insert(channelId, entries);
    };

    // Local session first so its flags stay current between presence updates.
    // Its own state is authoritative: a live flag for the same user may come
    // from another device signed into the same account.
    if (showLocal)
        appendEntry(localUserId, m_localState, QHash<QString, QJsonObject>());

    const QHash<QString, QJsonObject> liveFlags = m_liveFlagsByTopic.value(m_watchTopic);
    for (auto keyIt = metas.begin(); keyIt != metas.end(); ++keyIt) {
        for (auto refIt = keyIt.value().begin(); refIt != keyIt.value().end(); ++refIt) {
            const QJsonObject &payload = refIt.value();
            QString userId = payload.value(QStringLiteral("userId")).toString();
            if (userId.isEmpty())
                userId = keyIt.key();
            if (showLocal && userId == localUserId)
                continue;
            appendEntry(keyIt.key(), payload, liveFlags);
        }
    }

    if (presence == m_presence)
        return;
    m_presence = presence;
    emit presenceChanged();
}
