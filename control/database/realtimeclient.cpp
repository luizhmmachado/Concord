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
    m_accessToken = accessToken;

    QUrl wsUrl(QUrl(projectUrl).toString().replace("http", "ws") + "/realtime/v1/websocket");
    QUrlQuery query;
    query.addQueryItem("apikey", anonKey);
    query.addQueryItem("vsn", "1.0.0");
    wsUrl.setQuery(query);

    m_socket.open(wsUrl);
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

void RealtimeClient::sendMessage(const QString &topic, const QString &event, const QJsonObject &payload, const QString &joinRef)
{
    QJsonObject message{
        {"topic", topic},
        {"event", event},
        {"payload", payload},
        {"ref", QString::number(nextRef())},
    };
    if (!joinRef.isEmpty())
        message["join_ref"] = joinRef;

    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void RealtimeClient::joinTopic(const QString &topic)
{
    if (m_joinedTopics.contains(topic))
        return;

    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        if (!m_pendingTopics.contains(topic))
            m_pendingTopics.append(topic);
        return;
    }

    const QJsonObject payload{
        {"config", QJsonObject{{"broadcast", QJsonObject{{"ack", false}, {"self", false}}},
                                {"presence", QJsonObject{{"key", ""}}},
                                {"postgres_changes", QJsonArray()}}}
    };
    sendMessage(topic, "phx_join", payload, QString::number(nextRef()));
    m_joinedTopics.append(topic);
}

void RealtimeClient::leaveTopic(const QString &topic)
{
    if (!m_joinedTopics.removeOne(topic))
        return;
    sendMessage(topic, "phx_leave", QJsonObject());
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
    const QStringList pending = m_pendingTopics;
    m_pendingTopics.clear();
    for (const QString &topic : pending)
        joinTopic(topic);
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

    if (event == "phx_reply" && payload.value("status").toString() == "ok") {
        emit topicJoined(topic);
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
