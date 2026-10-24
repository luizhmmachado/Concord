#include "realtimeclient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

#include <QDebug>
RealtimeClient::RealtimeClient(QObject *parent)
    : QObject(parent)
{
    qInfo().noquote() << "RealtimeClient::RealtimeClient";
    connect(&m_socket, &QWebSocket::connected, this, &RealtimeClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &RealtimeClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &RealtimeClient::onTextMessageReceived);

    m_heartbeatTimer.setInterval(25000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &RealtimeClient::sendHeartbeat);
}

RealtimeClient::~RealtimeClient()
{
    qInfo().noquote() << "RealtimeClient::~RealtimeClient";
    // QWebSocket is a value member (not heap-allocated), so it is destroyed
    // automatically; explicitly close first to send a clean close frame.
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.close();
}

void RealtimeClient::connectToServer(const QString &projectUrl, const QString &anonKey, const QString &accessToken)
{
    qInfo().noquote() << "RealtimeClient::connectToServer" << "projectUrl=" << projectUrl;
    m_projectUrl = projectUrl;
    m_anonKey = anonKey;

    // A reconnect would drop every subscription; just refresh the credentials.
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        setAccessToken(accessToken);
        return;
    }

    m_accessToken = accessToken;

    QUrl wsUrl(QUrl(projectUrl).toString().replace("http", "ws") + "/realtime/v1/websocket");
    QUrlQuery query;
    query.addQueryItem("apikey", anonKey);
    query.addQueryItem("vsn", "1.0.0");
    wsUrl.setQuery(query);

    m_socket.open(wsUrl);
}

void RealtimeClient::setAccessToken(const QString &accessToken)
{
    qInfo().noquote() << "RealtimeClient::setAccessToken";
    if (m_accessToken == accessToken)
        return;
    m_accessToken = accessToken;

    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    for (const QString &topic : m_joinedTopics)
        sendMessage(topic, "access_token", QJsonObject{{"access_token", m_accessToken}});
}

void RealtimeClient::disconnectFromServer()
{
    qInfo().noquote() << "RealtimeClient::disconnectFromServer";
    m_heartbeatTimer.stop();
    m_socket.close();
}

int RealtimeClient::nextRef()
{
    qInfo().noquote() << "RealtimeClient::nextRef";
    return ++m_refCounter;
}

void RealtimeClient::sendMessage(const QString &topic, const QString &event, const QJsonObject &payload)
{
    qInfo().noquote() << "RealtimeClient::sendMessage" << "topic=" << topic << "event=" << event << "payload=" << payload;
    const QString ref = QString::number(nextRef());
    QJsonObject message{
        {"topic", topic},
        {"event", event},
        {"payload", payload},
        {"ref", ref},
    };

    if (event == QLatin1String("phx_join")) {
        m_topicJoinRefs.insert(topic, ref);
        message["join_ref"] = ref;
    } else if (m_topicJoinRefs.contains(topic)) {
        // Without this the server answers "unmatched topic".
        message["join_ref"] = m_topicJoinRefs.value(topic);
    }

    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void RealtimeClient::joinTopic(const QString &topic, const QJsonArray &postgresChanges,
                               const QString &presenceKey)
{
    qInfo().noquote() << "RealtimeClient::joinTopic" << "topic=" << topic
                      << "postgresChanges=" << postgresChanges
                      << "presence=" << !presenceKey.isEmpty();
    if (!m_requestedTopics.contains(topic))
        m_requestedTopics.append(topic);

    // Rejoins (reconnect) pass no key; keep the one registered originally.
    if (!presenceKey.isEmpty())
        m_presenceKeys.insert(topic, presenceKey);

    if (m_joinedTopics.contains(topic))
        return;

    if (!postgresChanges.isEmpty())
        m_topicChanges.insert(topic, postgresChanges);

    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        if (!m_pendingTopics.contains(topic))
            m_pendingTopics.append(topic);
        return;
    }

    QJsonObject config{
        {"broadcast", QJsonObject{{"ack", false}, {"self", false}}},
        {"postgres_changes", m_topicChanges.value(topic)},
    };
    const QString key = m_presenceKeys.value(topic);
    if (!key.isEmpty())
        config.insert("presence", QJsonObject{{"enabled", true}, {"key", key}});

    const QJsonObject payload{
        {"config", config},
        {"access_token", m_accessToken},
    };
    sendMessage(topic, "phx_join", payload);
    m_joinedTopics.append(topic);
}

void RealtimeClient::leaveTopic(const QString &topic)
{
    qInfo().noquote() << "RealtimeClient::leaveTopic" << "topic=" << topic;
    m_requestedTopics.removeOne(topic);
    m_pendingTopics.removeOne(topic);
    m_topicChanges.remove(topic);
    m_presenceKeys.remove(topic);
    m_presenceStates.remove(topic);
    m_acknowledgedTopics.remove(topic);
    if (!m_joinedTopics.removeOne(topic))
        return;
    sendMessage(topic, "phx_leave", QJsonObject());
    m_topicJoinRefs.remove(topic);
}

void RealtimeClient::trackPresence(const QString &topic, const QJsonObject &state)
{
    qInfo().noquote() << "RealtimeClient::trackPresence" << "topic=" << topic;
    // Keep the latest state even before the join is acknowledged; it is sent
    // from topicJoined() and again after every reconnect.
    m_presenceStates.insert(topic, state);
    if (m_acknowledgedTopics.contains(topic))
        sendPresence(topic, QStringLiteral("track"), state);
}

void RealtimeClient::untrackPresence(const QString &topic)
{
    qInfo().noquote() << "RealtimeClient::untrackPresence" << "topic=" << topic;
    m_presenceStates.remove(topic);
    if (m_acknowledgedTopics.contains(topic))
        sendPresence(topic, QStringLiteral("untrack"));
}

void RealtimeClient::sendPresence(const QString &topic, const QString &event, const QJsonObject &state)
{
    QJsonObject payload{
        {"type", "presence"},
        {"event", event},
    };
    if (event == QLatin1String("track"))
        payload.insert("payload", state);
    sendMessage(topic, "presence", payload);
}

void RealtimeClient::broadcast(const QString &topic, const QString &event, const QJsonObject &payload)
{
    qInfo().noquote() << "RealtimeClient::broadcast" << "topic=" << topic << "event=" << event << "payload=" << payload;
    const QJsonObject message{
        {"type", "broadcast"},
        {"event", event},
        {"payload", payload},
    };
    sendMessage(topic, "broadcast", message);
}

void RealtimeClient::onConnected()
{
    qInfo().noquote() << "RealtimeClient::onConnected";
    m_heartbeatTimer.start();
    // Re-subscribe everything: the server keeps no state across reconnects.
    const QStringList topics = m_requestedTopics;
    m_pendingTopics.clear();
    for (const QString &topic : topics)
        joinTopic(topic, m_topicChanges.value(topic));
    emit connected();
}

void RealtimeClient::onDisconnected()
{
    qInfo().noquote() << "RealtimeClient::onDisconnected";
    m_heartbeatTimer.stop();
    m_joinedTopics.clear();
    m_acknowledgedTopics.clear();
    emit disconnected();
}

void RealtimeClient::sendHeartbeat()
{
    qInfo().noquote() << "RealtimeClient::sendHeartbeat";
    sendMessage("phoenix", "heartbeat", QJsonObject());
}

void RealtimeClient::onTextMessageReceived(const QString &message)
{
    qInfo().noquote() << "RealtimeClient::onTextMessageReceived" << "message=" << message;
    const QJsonObject root = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString topic = root.value("topic").toString();
    const QString event = root.value("event").toString();
    const QJsonObject payload = root.value("payload").toObject();

    if (event == "phx_close" || event == "phx_error") {
        // Rejoin with a delay: an immediate retry can trip server side limits.
        qWarning() << "[realtime] canal fechado:" << topic << event;
        m_joinedTopics.removeOne(topic);
        m_topicJoinRefs.remove(topic);
        m_acknowledgedTopics.remove(topic);
        if (m_requestedTopics.contains(topic)) {
            QTimer::singleShot(5000, this, [this, topic]() {
                joinTopic(topic, m_topicChanges.value(topic));
            });
        }
        return;
    }

    if (event == "system" && payload.value("status").toString() == "error") {
        qWarning() << "[realtime]" << topic << payload.value("message").toString();
        return;
    }

    if (event == "phx_reply" && payload.value("status").toString() == "ok") {
        // Only the reply carrying the join ref means the channel is ready;
        // replies to presence/broadcast pushes also arrive as "ok" here.
        const QString ref = root.value("ref").toString();
        if (!ref.isEmpty() && ref != m_topicJoinRefs.value(topic))
            return;
        m_acknowledgedTopics.insert(topic);
        emit topicJoined(topic);
        // Presence state is not persisted by the server across reconnects.
        if (m_presenceStates.contains(topic))
            sendPresence(topic, QStringLiteral("track"), m_presenceStates.value(topic));
        return;
    }
    if (event == "phx_reply") {
        qWarning() << "[realtime] phx_reply erro em" << topic
                   << QJsonDocument(payload).toJson(QJsonDocument::Compact);
        // An error replying to a track/broadcast push is not a failed join;
        // only a join rejection means the subscription never became active.
        const QString ref = root.value("ref").toString();
        if (!ref.isEmpty() && ref != m_topicJoinRefs.value(topic))
            return;
        // A rejected join used to stay in m_joinedTopics forever, so the
        // subscription was silently dead. Drop it and retry while wanted.
        if (m_joinedTopics.removeOne(topic) && m_requestedTopics.contains(topic)) {
            m_topicJoinRefs.remove(topic);
            QTimer::singleShot(5000, this, [this, topic]() {
                if (m_requestedTopics.contains(topic) && !m_joinedTopics.contains(topic))
                    joinTopic(topic, m_topicChanges.value(topic));
            });
        }
        return;
    }
    if (event == "presence_state") {
        emit presenceState(topic, payload);
        return;
    }
    if (event == "presence_diff") {
        emit presenceDiff(topic, payload);
        return;
    }
    if (event == "broadcast") {
        emit broadcastReceived(topic, payload.value("event").toString(), payload.value("payload").toObject());
        return;
    }
    if (event == "postgres_changes") {
        const QJsonObject data = payload.value("data").toObject();
        emit postgresChange(topic, data.value("type").toString(), data.value("record").toObject());
        return;
    }
}
