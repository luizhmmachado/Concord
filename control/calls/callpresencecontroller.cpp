#include "callpresencecontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>

CallPresenceController::CallPresenceController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    m_refreshDebounce.setSingleShot(true);
    m_refreshDebounce.setInterval(250);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &CallPresenceController::refresh);

    // Deleting stale rows is what makes other clients receive a realtime event.
    m_sweepTimer.setInterval(20000);
    connect(&m_sweepTimer, &QTimer::timeout, this, [this]() {
        m_client->rpc("sweep_stale_call_participants", QJsonObject{}, "presence:sweep");
    });

    connect(m_client, &SupabaseClient::rpcFinished, this, [this](const QString &tag, bool ok, const QJsonValue &result, const QString &) {
        if (tag == "presence:sweep") {
            if (ok)
                m_refreshDebounce.start();
            return;
        }
        if (tag != "presence:list" || !ok)
            return;

        QVariantMap presence;
        for (const QJsonValue &value : result.toArray()) {
            const QJsonObject row = value.toObject();
            const QString channelId = row.value("channel_id").toString();

            QVariantList entries = presence.value(channelId).toList();
            entries.append(QVariantMap{
                {"userId", row.value("user_id").toString()},
                {"displayName", row.value("display_name").toString()},
                {"muted", row.value("is_muted").toBool()},
                {"cameraOn", row.value("is_camera_on").toBool()},
                {"screenSharing", row.value("is_screen_sharing").toBool()},
            });
            presence.insert(channelId, entries);
        }

        if (presence == m_presence)
            return;
        m_presence = presence;
        emit presenceChanged();
    });

    connect(m_realtime, &RealtimeClient::postgresChange, this, [this](const QString &topic, const QString &, const QJsonObject &) {
        // DELETE payloads carry no "record", so react to the topic instead of
        // inspecting the row; otherwise leaving a call never refreshes.
        if (topic == QLatin1String("realtime:concord:call_participants"))
            m_refreshDebounce.start();
    });
}

void CallPresenceController::watchServer(const QString &serverId)
{
    if (serverId.isEmpty())
        return;

    m_serverId = serverId;

    if (!m_realtimeSubscribed) {
        m_realtime->joinTopic("realtime:concord:call_participants",
                              QJsonArray{QJsonObject{{"event", "*"},
                                                     {"schema", "public"},
                                                     {"table", "call_participants"}}});
        m_realtimeSubscribed = true;
    }

    m_sweepTimer.start();
    refresh();
}

void CallPresenceController::refresh()
{
    if (m_serverId.isEmpty())
        return;
    m_client->rpc("list_server_call_participants", QJsonObject{{"p_server_id", m_serverId}}, "presence:list");
}
