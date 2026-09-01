#include "chatcontroller.h"

ChatController::ChatController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag != "chat:history")
            return;
        if (!ok) {
            emit errorOccurred(error);
            return;
        }
        m_messages.setMessages(rows);
    });

    connect(m_client, &SupabaseClient::insertFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag != "chat:send")
            return;
        if (!ok) {
            emit errorOccurred(error);
            return;
        }
        // Our own message: append locally too (Realtime broadcast excludes
        // the sender by default, see RealtimeClient "self": false config).
        if (!rows.isEmpty())
            m_messages.appendMessage(rows.first().toObject());
    });

    connect(m_realtime, &RealtimeClient::postgresChange, this, [this](const QString &topic, const QString &eventType, const QJsonObject &record) {
        Q_UNUSED(topic)
        if (eventType != "INSERT")
            return;
        if (record.value("channel_id").toString() != m_currentChannelId)
            return;
        m_messages.appendMessage(record);
    });
}

void ChatController::openChannel(const QString &channelId)
{
    m_currentChannelId = channelId;
    emit currentChannelChanged();
    m_messages.clear();
    m_client->select(QStringLiteral("messages?channel_id=eq.%1&select=*,profiles(username,display_name)&order=created_at.asc").arg(channelId),
                      "chat:history");

    if (!m_realtimeSubscribed) {
        m_realtime->joinTopic("realtime:public:messages");
        m_realtimeSubscribed = true;
    }
}

void ChatController::sendMessage(const QString &content)
{
    if (content.trimmed().isEmpty() || m_currentChannelId.isEmpty())
        return;

    m_client->insert("messages",
                      QJsonObject{{"channel_id", m_currentChannelId}, {"author_id", m_client->currentUserId()}, {"content", content}},
                      "chat:send");
}
