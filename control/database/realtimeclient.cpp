#include "realtimeclient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

RealtimeClient::RealtimeClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QWebSocket::connected, this, &RealtimeClient::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &RealtimeClient::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &RealtimeClient::onTextMessageReceived);

    m_heartbeatTimer.setInterval(25000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &RealtimeClient::sendHeartbeat);
}

RealtimeClient::~RealtimeClient()
{
    // QWebSocket is a value member (not heap-allocated), so it is destroyed
    // automatically; explicitly close first to send a clean close frame.
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.close();
}

void RealtimeClient::connectToServer(const QString &projectUrl, const QString &anonKey, const QString &accessToken)
{
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
    m_heartbeatTimer.stop();
    m_socket.close();
}

int RealtimeClient::nextRef()
{
    return ++m_refCounter;
}

void RealtimeClient::sendMessage(const QString &topic, const QString &event, const QJsonObject &payload)
{
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

void RealtimeClient::joinTopic(const QString &topic, const QJsonArray &postgresChanges)
{
    if (!m_requestedTopics.contains(topic))
        m_requestedTopics.append(topic);

    if (m_joinedTopics.contains(topic))
        return;

    if (!postgresChanges.isEmpty())
        m_topicChanges.insert(topic, postgresChanges);

    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        if (!m_pendingTopics.contains(topic))
            m_pendingTopics.append(topic);
        return;
    }

    const QJsonObject payload{
        {"config", QJsonObject{{"broadcast", QJsonObject{{"ack", false}, {"self", false}}},
                                {"postgres_changes", m_topicChanges.value(topic)}}},
        {"access_token", m_accessToken},
    };
    sendMessage(topic, "phx_join", payload);
    m_joinedTopics.append(topic);
}

void RealtimeClient::leaveTopic(const QString &topic)
{
    m_requestedTopics.removeOne(topic);
    m_pendingTopics.removeOne(topic);
    m_topicChanges.remove(topic);
    if (!m_joinedTopics.removeOne(topic))
        return;
    sendMessage(topic, "phx_leave", QJsonObject());
    m_topicJoinRefs.remove(topic);
}

void RealtimeClient::broadcast(const QString &topic, const QString &event, const QJsonObject &payload)
{
    const QJsonObject message{
        {"type", "broadcast"},
        {"event", event},
        {"payload", payload},
    };
    sendMessage(topic, "broadcast", message);
}

void RealtimeClient::onConnected()
{
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
    m_heartbeatTimer.stop();
    m_joinedTopics.clear();
    emit disconnected();
}

void RealtimeClient::sendHeartbeat()
{
    sendMessage("phoenix", "heartbeat", QJsonObject());
}

void RealtimeClient::onTextMessageReceived(const QString &message)
{
    const QJsonObject root = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString topic = root.value("topic").toString();
    const QString event = root.value("event").toString();
    const QJsonObject payload = root.value("payload").toObject();

    if (event == "phx_close" || event == "phx_error") {
        // Rejoin with a delay: an immediate retry can trip server side limits.
        qWarning() << "[realtime] canal fechado:" << topic << event;
        m_joinedTopics.removeOne(topic);
        m_topicJoinRefs.remove(topic);
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
        emit topicJoined(topic);
        return;
    }
    if (event == "phx_reply") {
        qWarning() << "[realtime] phx_reply erro em" << topic
                   << QJsonDocument(payload).toJson(QJsonDocument::Compact);
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
