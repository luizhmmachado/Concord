#ifndef CALLPRESENCECONTROLLER_H
#define CALLPRESENCECONTROLLER_H

#include <QObject>
#include <QVariantMap>
#include <QTimer>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"

// Who is currently in each voice channel of the selected server, so the channel
// list can show it without the user having to join the call first.
//
// Backed by call_participants plus a heartbeat: Realtime Presence was tried but
// this project's presence rate limit rejects the channel join itself once
// tripped, which left the list permanently empty.
class CallPresenceController : public QObject
{
    Q_OBJECT
    // channelId -> list of {userId, displayName, muted, cameraOn, screenSharing}
    Q_PROPERTY(QVariantMap presence READ presence NOTIFY presenceChanged)

public:
    CallPresenceController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);

    QVariantMap presence() const { return m_presence; }

    Q_INVOKABLE void watchServer(const QString &serverId);
    Q_INVOKABLE void refresh();

signals:
    void presenceChanged();

private:
    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    QVariantMap m_presence;
    QString m_serverId;
    // Realtime fires once per row; collapse bursts into a single refetch.
    QTimer m_refreshDebounce;
    QTimer m_sweepTimer;
    bool m_realtimeSubscribed = false;
};

#endif // CALLPRESENCECONTROLLER_H
