#ifndef CALLPRESENCECONTROLLER_H
#define CALLPRESENCECONTROLLER_H

#include <QObject>
#include <QVariantMap>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"

// Who is currently in each voice channel of the selected server.
//
// Mirrors Discord's model: this is ephemeral state owned by the realtime
// server, not a database table. Every client tracks its own voice session on
// its server's presence topic (realtime:presence:<serverId>); subscribers get
// presence_state on join and presence_diff afterwards, and the server removes
// the entry when the websocket disconnects. No heartbeat, sweep or polling.
//
// Only membership changes consume the per-client presence quota (5 calls /
// 30s), so mute/camera/screen updates travel as broadcasts on the same topic.
// A late subscriber asks for those flags with a `voice_state_request`
// broadcast, since presence only stores the state from the last track.
class CallPresenceController : public QObject
{
    Q_OBJECT
    // channelId -> list of {userId, displayName, muted, cameraOn, screenSharing}
    Q_PROPERTY(QVariantMap presence READ presence NOTIFY presenceChanged)

public:
    CallPresenceController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);

    QVariantMap presence() const { return m_presence; }

    Q_INVOKABLE void watchServer(const QString &serverId);

    // Publishes the local voice session. `serverId` is the server that owns the
    // voice channel; passing an empty channelId clears the session.
    Q_INVOKABLE void setLocalCallState(const QString &serverId, const QString &channelId,
                                       const QString &displayName, bool muted,
                                       bool cameraOn, bool screenSharing);
    Q_INVOKABLE void clearLocalCallState();

signals:
    void presenceChanged();

private:
    using RefMap = QHash<QString, QJsonObject>;              // phx_ref -> payload
    using MetaMap = QHash<QString, RefMap>;                  // presence key -> RefMap

    static QString topicForServer(const QString &serverId);
    static RefMap metasFrom(const QJsonValue &value);
    void applyPresenceState(const QString &topic, const QJsonObject &state);
    void applyPresenceDiff(const QString &topic, const QJsonObject &diff);
    void rebuildPresence();

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    QVariantMap m_presence;

    QString m_watchServerId;
    QString m_watchTopic;
    // Topic where our own session is tracked; empty while not in a call.
    QString m_publishTopic;
    QString m_localChannelId;
    QJsonObject m_localState;

    // Presence is kept per topic so switching servers never loses the snapshot
    // of the server we are still in a call on.
    QHash<QString, MetaMap> m_metasByTopic;
    QHash<QString, QHash<QString, QJsonObject>> m_liveFlagsByTopic;
};

#endif // CALLPRESENCECONTROLLER_H
