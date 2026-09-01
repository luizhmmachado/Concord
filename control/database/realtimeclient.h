#ifndef REALTIMECLIENT_H
#define REALTIMECLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QPointer>

// Minimal client for Supabase Realtime (Phoenix channels protocol).
// Used both for postgres_changes subscriptions (chat) and for broadcast-based
// signaling (WebRTC SDP/ICE exchange) on ephemeral topics.
class RealtimeClient : public QObject
{
    Q_OBJECT
public:
    explicit RealtimeClient(QObject *parent = nullptr);
    ~RealtimeClient() override;

    void connectToServer(const QString &projectUrl, const QString &anonKey, const QString &accessToken);
    void disconnectFromServer();

    // Joins a realtime topic (e.g. "realtime:public:messages" or "call:<id>").
    void joinTopic(const QString &topic);
    void leaveTopic(const QString &topic);

    // Sends a broadcast event payload on an already-joined topic.
    void broadcast(const QString &topic, const QString &event, const QJsonObject &payload);

signals:
    void connected();
    void disconnected();
    void topicJoined(const QString &topic);
    void broadcastReceived(const QString &topic, const QString &event, const QJsonObject &payload);
    void postgresChange(const QString &topic, const QString &eventType, const QJsonObject &record);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void sendHeartbeat();

private:
    void sendMessage(const QString &topic, const QString &event, const QJsonObject &payload, const QString &joinRef = QString());
    int nextRef();

    QWebSocket m_socket;
    QTimer m_heartbeatTimer;
    QString m_projectUrl;
    QString m_anonKey;
    QString m_accessToken;
    int m_refCounter = 0;
    QStringList m_pendingTopics;
    QStringList m_joinedTopics;
};

#endif // REALTIMECLIENT_H
