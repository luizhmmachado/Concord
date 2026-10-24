#include "chatcontroller.h"

#include <QDebug>
ChatController::ChatController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    qInfo().noquote() << "ChatController::ChatController" << "client=" << client << "realtime=" << realtime;
    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag == "chat:hydrate") {
            if (ok && !rows.isEmpty())
                m_messages.appendMessage(rows.first().toObject());
            return;
        }
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
        if (rows.isEmpty())
            return;
        // Our own message: append locally too (Realtime broadcast excludes
        // the sender by default, see RealtimeClient "self": false config).
        // Ignore the response if the user switched channels while it was in flight.
        const QJsonObject row = rows.first().toObject();
        if (row.value("channel_id").toString() != m_currentChannelId)
            return;
        m_messages.appendMessage(row);
    });

    connect(m_client, &SupabaseClient::imageUploadFinished, this, [this](const QString &tag, bool ok, const QString &publicUrl, const QString &error) {
        if (tag != "chat:image")
            return;
        if (!ok) {
            setUploading(false);
            emit errorOccurred(error);
            return;
        }

        const QString channelId = m_pendingImageChannelId;
        const QString caption = m_pendingImageCaption;
        m_pendingImageChannelId.clear();
        m_pendingImageCaption.clear();
        setUploading(false);

        if (channelId.isEmpty() || publicUrl.isEmpty()) {
            emit errorOccurred(QStringLiteral("Não foi possível enviar a imagem."));
            return;
        }

        m_client->insert("messages?select=*,profiles(username,display_name)",
                         QJsonObject{{"channel_id", channelId},
                                     {"author_id", m_client->currentUserId()},
                                     {"content", caption},
                                     {"image_url", publicUrl}},
                         "chat:send");
    });

    connect(m_realtime, &RealtimeClient::postgresChange, this, [this](const QString &topic, const QString &eventType, const QJsonObject &record) {
        Q_UNUSED(topic)
        if (eventType != "INSERT")
            return;
        if (record.value("channel_id").toString() != m_currentChannelId)
            return;
        // The realtime payload has no joined profile row, so re-read the
        // message with its author; appendMessage() merges it by id.
        m_messages.appendMessage(record);
        m_client->select(QStringLiteral("messages?id=eq.%1&select=*,profiles(username,display_name)").arg(record.value("id").toString()),
                         "chat:hydrate");
    });
}

void ChatController::openChannel(const QString &channelId)
{
    qInfo().noquote() << "ChatController::openChannel" << "channelId=" << channelId;
    m_currentChannelId = channelId;
    emit currentChannelChanged();
    m_messages.clear();
    m_client->select(QStringLiteral("messages?channel_id=eq.%1&select=*,profiles(username,display_name)&order=created_at.asc").arg(channelId),
                      "chat:history");

    if (!m_realtimeSubscribed) {
        m_realtime->joinTopic("realtime:concord:messages",
                              QJsonArray{QJsonObject{{"event", "INSERT"},
                                                     {"schema", "public"},
                                                     {"table", "messages"}}});
        m_realtimeSubscribed = true;
    }
}

void ChatController::sendMessage(const QString &content)
{
    qInfo().noquote() << "ChatController::sendMessage" << "content=" << content;
    if (content.trimmed().isEmpty() || m_currentChannelId.isEmpty())
        return;

    m_client->insert("messages?select=*,profiles(username,display_name)",
                      QJsonObject{{"channel_id", m_currentChannelId}, {"author_id", m_client->currentUserId()}, {"content", content}},
                      "chat:send");
}

void ChatController::sendImageMessage(const QString &caption, const QString &filePath)
{
    qInfo().noquote() << "ChatController::sendImageMessage" << "caption=" << caption << "filePath=" << filePath;
    if (m_uploading || m_currentChannelId.isEmpty() || filePath.isEmpty())
        return;

    m_pendingImageChannelId = m_currentChannelId;
    m_pendingImageCaption = caption;
    setUploading(true);
    m_client->uploadChatImage(filePath, "chat:image");
}

void ChatController::setUploading(bool uploading)
{
    qInfo().noquote() << "ChatController::setUploading" << "uploading=" << uploading;
    if (m_uploading == uploading)
        return;
    m_uploading = uploading;
    emit uploadingChanged();
}
