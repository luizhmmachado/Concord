#ifndef SERVERCONTROLLER_H
#define SERVERCONTROLLER_H

#include <QObject>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "serverlistmodel.h"
#include "channellistmodel.h"

// Manages the list of servers (guilds) the user belongs to and the channels
// inside the currently selected server.
class ServerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ServerListModel *servers READ servers CONSTANT)
    Q_PROPERTY(ChannelListModel *channels READ channels CONSTANT)
    Q_PROPERTY(QString currentServerId READ currentServerId NOTIFY currentServerChanged)
    Q_PROPERTY(QString currentServerName READ currentServerName NOTIFY currentServerChanged)
    Q_PROPERTY(QString currentServerInviteCode READ currentServerInviteCode NOTIFY currentServerChanged)

public:
    ServerController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);

    ServerListModel *servers() { return &m_servers; }
    ChannelListModel *channels() { return &m_channels; }
    QString currentServerId() const { return m_currentServerId; }
    QString currentServerName() const;
    QString currentServerInviteCode() const;

    Q_INVOKABLE void loadServers(const QString &userId);
    Q_INVOKABLE void selectServer(const QString &serverId);
    Q_INVOKABLE void createServer(const QString &name);
    Q_INVOKABLE void joinServerByInviteCode(const QString &inviteCode);
    Q_INVOKABLE void createChannel(const QString &serverId, const QString &name, const QString &type);

signals:
    void currentServerChanged();
    void serverCreated(const QString &serverId);
    void serverJoined(const QString &serverId);
    void errorOccurred(const QString &message);

private:
    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    ServerListModel m_servers;
    ChannelListModel m_channels;
    QString m_currentServerId;
    QString m_userId;
};

#endif // SERVERCONTROLLER_H
