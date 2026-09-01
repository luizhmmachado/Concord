#ifndef CHATCONTROLLER_H
#define CHATCONTROLLER_H

#include <QObject>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "messagelistmodel.h"

// Loads/sends messages for the currently open text channel and applies
// realtime inserts as they arrive from other clients.
class ChatController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(MessageListModel *messages READ messages CONSTANT)
    Q_PROPERTY(QString currentChannelId READ currentChannelId NOTIFY currentChannelChanged)

public:
    ChatController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);

    MessageListModel *messages() { return &m_messages; }
    QString currentChannelId() const { return m_currentChannelId; }

    Q_INVOKABLE void openChannel(const QString &channelId);
    Q_INVOKABLE void sendMessage(const QString &content);

signals:
    void currentChannelChanged();
    void errorOccurred(const QString &message);

private:
    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    MessageListModel m_messages;
    QString m_currentChannelId;
    bool m_realtimeSubscribed = false;
};

#endif // CHATCONTROLLER_H
