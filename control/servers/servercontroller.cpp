#include "servercontroller.h"

#include <QJsonDocument>

#include <QDebug>
ServerController::ServerController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    qInfo().noquote() << "ServerController::ServerController" << "client=" << client << "realtime=" << realtime;
    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag == "servers:list") {
            if (!ok) {
                emit errorOccurred(error);
                return;
            }
            QJsonArray flattened;
            for (const QJsonValue &row : rows) {
                const QJsonValue server = row.toObject().value("servers");
                if (server.isObject())
                    flattened.append(server);
            }
            m_servers.setServers(flattened);
            // The selected server's name only becomes known once the list
            // arrives.
            emit currentServerChanged();
        } else if (tag == "channels:list") {
            if (!ok) {
                emit errorOccurred(error);
                return;
            }
            m_channels.setChannels(rows);
        }
    });

    connect(m_client, &SupabaseClient::rpcFinished, this, [this](const QString &tag, bool ok, const QJsonValue &result, const QString &error) {
        if (tag == QStringLiteral("servers:joinInvite")) {
            if (!ok || !result.isString()) {
                const QString reason = QJsonDocument::fromJson(error.toUtf8()).object().value("message").toString();
                emit errorOccurred(reason.isEmpty() ? QStringLiteral("Convite inv\u00e1lido") : reason);
                return;
            }
            emit serverJoined(result.toString());
            loadServers(m_userId);
            return;
        }

        const QString invitePrefix = QStringLiteral("servers:inviteCode:");
        if (!tag.startsWith(invitePrefix))
            return;

        const QString requestServerId = tag.mid(invitePrefix.size());
        if (requestServerId != m_currentServerId)
            return;

        setInviteCodeLoading(false);
        if (!ok || !result.isString()) {
            const QString reason = QJsonDocument::fromJson(error.toUtf8()).object().value("message").toString();
            emit errorOccurred(reason.isEmpty() ? QStringLiteral("N\u00e3o foi poss\u00edvel gerar o convite") : reason);
            return;
        }

        m_currentInviteCode = result.toString();
        emit currentServerInviteChanged();
    });

    connect(m_client, &SupabaseClient::insertFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag == "servers:create") {
            if (!ok || rows.isEmpty()) {
                emit errorOccurred(error);
                return;
            }
            const QJsonObject server = rows.first().toObject();
            m_servers.upsertServer(server);
            // Owner membership is now created atomically by an "on_server_created"
            // DB trigger (see migration 0005) - no follow-up insert needed here.
            emit serverCreated(server.value("id").toString());
        } else if (tag == "channels:create") {
            if (!ok || rows.isEmpty()) {
                emit errorOccurred(error);
                return;
            }
            m_channels.addChannel(rows.first().toObject());
        }
    });
}

void ServerController::loadServers(const QString &userId)
{
    qInfo().noquote() << "ServerController::loadServers" << "userId=" << userId;
    m_userId = userId;
    m_client->select(QStringLiteral("server_members?user_id=eq.%1&select=servers(id,name,icon_url,owner_id)").arg(userId), "servers:list");
}

void ServerController::selectServer(const QString &serverId)
{
    qInfo().noquote() << "ServerController::selectServer" << "serverId=" << serverId;
    const bool changed = serverId != m_currentServerId;
    m_currentServerId = serverId;
    if (changed) {
        m_currentInviteCode.clear();
        setInviteCodeLoading(false);
        emit currentServerInviteChanged();
    }
    emit currentServerChanged();
    m_client->select(QStringLiteral("channels?server_id=eq.%1&select=*&order=position.asc").arg(serverId), "channels:list");
}

void ServerController::createServer(const QString &name)
{
    qInfo().noquote() << "ServerController::createServer" << "name=" << name;
    m_client->insert("servers", QJsonObject{{"name", name}, {"owner_id", m_userId}}, "servers:create");
}

void ServerController::joinServerByInviteCode(const QString &inviteCode)
{
    qInfo().noquote() << "ServerController::joinServerByInviteCode";
    // RLS only exposes servers to their members, so a non-member cannot resolve
    // the invite code directly; a SECURITY DEFINER function does it instead.
    QString code = inviteCode.trimmed();
    const int separator = code.lastIndexOf('/');
    if (separator >= 0)
        code = code.mid(separator + 1);

    m_client->rpc("join_server_by_invite", QJsonObject{{"p_invite_code", code}}, "servers:joinInvite");
}

QString ServerController::currentServerName() const
{
    qInfo().noquote() << "ServerController::currentServerName";
    const ServerEntry *entry = m_servers.serverById(m_currentServerId);
    return entry ? entry->name : QString();
}

QString ServerController::currentServerInviteCode() const
{
    return m_currentInviteCode;
}

void ServerController::requestInviteCode()
{
    qInfo().noquote() << "ServerController::requestInviteCode" << "serverId=" << m_currentServerId;
    if (m_currentServerId.isEmpty())
        return;

    m_currentInviteCode.clear();
    emit currentServerInviteChanged();
    setInviteCodeLoading(true);
    m_client->rpc(QStringLiteral("create_server_invite"),
                  QJsonObject{{"p_server_id", m_currentServerId}},
                  QStringLiteral("servers:inviteCode:%1").arg(m_currentServerId));
}

void ServerController::setInviteCodeLoading(bool loading)
{
    if (loading == m_inviteCodeLoading)
        return;
    m_inviteCodeLoading = loading;
    emit inviteCodeLoadingChanged();
}

void ServerController::createChannel(const QString &serverId, const QString &name, const QString &type)
{
    qInfo().noquote() << "ServerController::createChannel" << "serverId=" << serverId << "name=" << name << "type=" << type;
    m_client->insert("channels", QJsonObject{{"server_id", serverId}, {"name", name}, {"type", type}}, "channels:create");
}
