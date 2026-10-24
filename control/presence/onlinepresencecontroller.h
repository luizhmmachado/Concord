#ifndef ONLINEPRESENCECONTROLLER_H
#define ONLINEPRESENCECONTROLLER_H

#include <QObject>
#include <QVariantMap>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>

#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"

// Global online roster backed by Realtime Presence.
//
// Every authenticated client tracks one entry on a single ephemeral topic, so
// server member lists can mark a user online no matter which server they are
// currently looking at. The entry disappears when the websocket drops, which
// gives Discord-like online/offline status without a database heartbeat.
class OnlinePresenceController : public QObject
{
    Q_OBJECT
    // userId -> displayName
    Q_PROPERTY(QVariantMap onlineUsers READ onlineUsers NOTIFY onlineUsersChanged)

public:
    OnlinePresenceController(SupabaseClient *client, RealtimeClient *realtime,
                             QObject *parent = nullptr);

    QVariantMap onlineUsers() const { return m_onlineUsers; }

    // Publishes the signed-in user. Safe to call before the websocket is
    // connected: the join/track are queued and replayed after every reconnect.
    Q_INVOKABLE void setLocalUser(const QString &userId, const QString &displayName);
    Q_INVOKABLE void clear();

signals:
    void onlineUsersChanged();

private:
    using RefMap = QHash<QString, QJsonObject>;
    using MetaMap = QHash<QString, RefMap>;

    static QString topic();
    static RefMap metasFrom(const QJsonValue &value);
    void applyPresenceState(const QJsonObject &state);
    void applyPresenceDiff(const QJsonObject &diff);
    void rebuildOnlineUsers();

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    QVariantMap m_onlineUsers;
    MetaMap m_metas;

    QString m_localUserId;
    QJsonObject m_localState;
    bool m_joined = false;
};

#endif // ONLINEPRESENCECONTROLLER_H
