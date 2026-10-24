#ifndef REALTIMECLIENT_H
#define REALTIMECLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QSet>
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

    // Realtime evaluates RLS with this token; postgres_changes deliver nothing
    // without it. Must be refreshed before the JWT expires.
    void setAccessToken(const QString &accessToken);

    // Joins a realtime topic. `postgresChanges` declares the table
    // subscriptions, e.g. [{event: "INSERT", schema: "public", table: "messages"}];
    // Supabase ignores changes that were not declared here. A non-empty
    // `presenceKey` enables Presence on the topic, keyed by that value.
    void joinTopic(const QString &topic, const QJsonArray &postgresChanges = QJsonArray(),
                   const QString &presenceKey = QString());
    void leaveTopic(const QString &topic);

    // Sends a broadcast event payload on an already-joined topic.
    void broadcast(const QString &topic, const QString &event, const QJsonObject &payload);

    // Ephemeral Presence: the server keeps the state in memory and removes it
    // when the websocket disconnects, so it never touches Postgres.
    void trackPresence(const QString &topic, const QJsonObject &state);
    void untrackPresence(const QString &topic);

signals:
    void connected();
    void disconnected();
    void topicJoined(const QString &topic);
    void broadcastReceived(const QString &topic, const QString &event, const QJsonObject &payload);
    void postgresChange(const QString &topic, const QString &eventType, const QJsonObject &record);
    // Raw Phoenix Presence payloads: {key: {metas: [...]}} and
    // {joins: {...}, leaves: {...}} respectively.
    void presenceState(const QString &topic, const QJsonObject &state);
    void presenceDiff(const QString &topic, const QJsonObject &diff);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void sendHeartbeat();

private:
    void sendMessage(const QString &topic, const QString &event, const QJsonObject &payload);
    void sendPresence(const QString &topic, const QString &event, const QJsonObject &state = QJsonObject());
    int nextRef();

    QWebSocket m_socket;
    QTimer m_heartbeatTimer;
    QString m_projectUrl;
    QString m_anonKey;
    QString m_accessToken;
    int m_refCounter = 0;
    QStringList m_pendingTopics;
    QStringList m_joinedTopics;
    QStringList m_requestedTopics;
    QHash<QString, QJsonArray> m_topicChanges;
    // Phoenix matches later messages to a channel by the join's ref.
    QHash<QString, QString> m_topicJoinRefs;
    // Presence is per channel: the key identifies us, the state is replayed on
    // every (re)join because the server forgets it when the socket drops.
    QHash<QString, QString> m_presenceKeys;
    QHash<QString, QJsonObject> m_presenceStates;
    QSet<QString> m_acknowledgedTopics;
};

#endif // REALTIMECLIENT_H
