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
            // The invite code / name of the selected server only become known
            // once the list arrives.
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
        if (tag != "servers:joinInvite")
            return;
        if (!ok || !result.isString()) {
            const QString reason = QJsonDocument::fromJson(error.toUtf8()).object().value("message").toString();
            emit errorOccurred(reason.isEmpty() ? QStringLiteral("Convite inv\u00e1lido") : reason);
            return;
        }
        emit serverJoined(result.toString());
        loadServers(m_userId);
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
    m_client->select(QStringLiteral("server_members?user_id=eq.%1&select=servers(*)").arg(userId), "servers:list");
}

void ServerController::selectServer(const QString &serverId)
{
    qInfo().noquote() << "ServerController::selectServer" << "serverId=" << serverId;
    m_currentServerId = serverId;
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
    qInfo().noquote() << "ServerController::joinServerByInviteCode" << "inviteCode=" << inviteCode;
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
    qInfo().noquote() << "ServerController::currentServerInviteCode";
    const ServerEntry *entry = m_servers.serverById(m_currentServerId);
    return entry ? entry->inviteCode : QString();
}

void ServerController::createChannel(const QString &serverId, const QString &name, const QString &type)
{
    qInfo().noquote() << "ServerController::createChannel" << "serverId=" << serverId << "name=" << name << "type=" << type;
    m_client->insert("channels", QJsonObject{{"server_id", serverId}, {"name", name}, {"type", type}}, "channels:create");
}
