#include "directmessagecontroller.h"

#include <QJsonDocument>
#include <QJsonObject>

DirectMessageController::DirectMessageController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    connect(m_client, &SupabaseClient::rpcFinished, this, [this](const QString &tag, bool ok, const QJsonValue &result, const QString &error) {
        if (tag == "dm:list") {
            setBusy(false);
            if (!ok) {
                emit errorOccurred(error);
                return;
            }
            m_conversations.setConversations(result.toArray());
            if (!m_pendingOpenConversationId.isEmpty()) {
                const QString conversationId = m_pendingOpenConversationId;
                m_pendingOpenConversationId.clear();
                openConversation(conversationId);
            }
        } else if (tag == "dm:start") {
            setBusy(false);
            if (!ok || !result.isString()) {
                // PostgREST reports RAISE EXCEPTION in a "message" field.
                const QString reason = QJsonDocument::fromJson(error.toUtf8()).object().value("message").toString();
                emit errorOccurred(reason.isEmpty() ? QStringLiteral("Não foi possível iniciar a conversa") : reason);
                return;
            }
            const QString conversationId = result.toString();
            m_pendingOpenConversationId = conversationId;
            emit friendAdded(conversationId);
            loadConversations();
        }
    });

    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag != "dm:history")
            return;
        if (!ok) {
            emit errorOccurred(error);
            return;
        }
        m_messages.setMessages(rows);
    });

    connect(m_client, &SupabaseClient::insertFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag != "dm:send")
            return;
        if (!ok) {
            emit errorOccurred(error);
            return;
        }
        if (rows.isEmpty())
            return;
        // Realtime excludes the sender, so append our own message locally.
        const QJsonObject row = rows.first().toObject();
        m_messages.appendMessage(row);
        m_conversations.setLastMessage(row.value("conversation_id").toString(),
                                       row.value("content").toString(),
                                       row.value("created_at").toString());
    });

    connect(m_realtime, &RealtimeClient::postgresChange, this, [this](const QString &topic, const QString &eventType, const QJsonObject &record) {
        Q_UNUSED(topic)
        if (eventType != "INSERT" || !record.contains("conversation_id"))
            return;

        const QString conversationId = record.value("conversation_id").toString();
        m_conversations.setLastMessage(conversationId,
                                       record.value("content").toString(),
                                       record.value("created_at").toString());

        if (conversationId == m_currentConversationId)
            m_messages.appendMessage(record);
    });
}

void DirectMessageController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void DirectMessageController::setLocalDisplayName(const QString &name)
{
    if (m_localDisplayName == name)
        return;
    m_localDisplayName = name;
    m_messages.setAuthorNameFallback(m_client->currentUserId(), name);
    emit localDisplayNameChanged();
}

void DirectMessageController::loadConversations()
{
    if (m_client->currentUserId().isEmpty())
        return;

    setBusy(true);
    m_client->rpc("list_direct_conversations", QJsonObject{}, "dm:list");

    if (!m_realtimeSubscribed) {
        m_realtime->joinTopic("realtime:concord:direct_messages",
                              QJsonArray{QJsonObject{{"event", "INSERT"},
                                                     {"schema", "public"},
                                                     {"table", "direct_messages"}}});
        m_realtimeSubscribed = true;
    }
}

void DirectMessageController::openConversation(const QString &conversationId)
{
    m_currentConversationId = conversationId;
    m_currentPeerName = m_conversations.peerNameFor(conversationId);
    emit currentConversationChanged();

    // Realtime payloads carry no joined profile row; in a 1:1 thread the author
    // is always one of the two participants.
    m_messages.setAuthorNameFallback(m_conversations.peerIdFor(conversationId), m_currentPeerName);
    m_messages.setAuthorNameFallback(m_client->currentUserId(), m_localDisplayName);

    m_messages.clear();
    m_client->select(QStringLiteral("direct_messages?conversation_id=eq.%1&select=*,profiles(username,display_name)&order=created_at.asc").arg(conversationId),
                     "dm:history");
}

void DirectMessageController::closeConversation()
{
    m_currentConversationId.clear();
    m_currentPeerName.clear();
    emit currentConversationChanged();
    m_messages.clear();
}

void DirectMessageController::sendMessage(const QString &content)
{
    if (content.trimmed().isEmpty() || m_currentConversationId.isEmpty())
        return;

    m_client->insert("direct_messages?select=*,profiles(username,display_name)",
                     QJsonObject{{"conversation_id", m_currentConversationId},
                                 {"author_id", m_client->currentUserId()},
                                 {"content", content}},
                     "dm:send");
}

void DirectMessageController::addFriend(const QString &usernameOrLink)
{
    QString username = usernameOrLink.trimmed();
    const int separator = username.lastIndexOf('/');
    if (separator >= 0)
        username = username.mid(separator + 1);
    username.remove('@');

    if (username.isEmpty()) {
        emit errorOccurred(QStringLiteral("Informe um usuário ou link de convite"));
        return;
    }

    setBusy(true);
    m_client->rpc("start_direct_conversation", QJsonObject{{"p_username", username}}, "dm:start");
}

void DirectMessageController::sendServerInvite(const QString &conversationId, const QString &inviteCode, const QString &serverName)
{
    if (conversationId.isEmpty() || inviteCode.isEmpty())
        return;

    const QString content = QStringLiteral("Entre no meu servidor \"%1\": concord://invite/%2")
                                .arg(serverName, inviteCode);

    m_client->insert("direct_messages?select=*,profiles(username,display_name)",
                     QJsonObject{{"conversation_id", conversationId},
                                 {"author_id", m_client->currentUserId()},
                                 {"content", content}},
                     "dm:send");
}
