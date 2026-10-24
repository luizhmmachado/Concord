#ifndef DIRECTMESSAGECONTROLLER_H
#define DIRECTMESSAGECONTROLLER_H

#include <QObject>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "control/chat/messagelistmodel.h"
#include "conversationlistmodel.h"

// Private 1:1 conversations: the conversation list, the open conversation's
// history, and adding friends from an invite link/username.
class DirectMessageController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConversationListModel *conversations READ conversations CONSTANT)
    Q_PROPERTY(MessageListModel *messages READ messages CONSTANT)
    Q_PROPERTY(QString currentConversationId READ currentConversationId NOTIFY currentConversationChanged)
    Q_PROPERTY(QString currentPeerName READ currentPeerName NOTIFY currentConversationChanged)
    Q_PROPERTY(bool uploading READ uploading NOTIFY uploadingChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString localDisplayName READ localDisplayName WRITE setLocalDisplayName NOTIFY localDisplayNameChanged)

public:
    DirectMessageController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);

    ConversationListModel *conversations() { return &m_conversations; }
    MessageListModel *messages() { return &m_messages; }
    QString currentConversationId() const { return m_currentConversationId; }
    QString currentPeerName() const { return m_currentPeerName; }
    bool uploading() const { return m_uploading; }
    bool busy() const { return m_busy; }
    QString localDisplayName() const { return m_localDisplayName; }
    void setLocalDisplayName(const QString &name);

    Q_INVOKABLE void loadConversations();
    Q_INVOKABLE void openConversation(const QString &conversationId);
    Q_INVOKABLE void closeConversation();
    Q_INVOKABLE void sendMessage(const QString &content);
    Q_INVOKABLE void sendImageMessage(const QString &caption, const QString &filePath);
    // Accepts a bare username or a "concord://user/<username>" invite link.
    Q_INVOKABLE void addFriend(const QString &usernameOrLink);
    // Sends a server invite link as a direct message.
    Q_INVOKABLE void sendServerInvite(const QString &conversationId, const QString &inviteCode, const QString &serverName);

signals:
    void currentConversationChanged();
    void uploadingChanged();
    void busyChanged();
    void localDisplayNameChanged();
    void friendAdded(const QString &conversationId);
    void errorOccurred(const QString &message);

private:
    void setBusy(bool busy);
    void setUploading(bool uploading);

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    ConversationListModel m_conversations;
    MessageListModel m_messages;
    QString m_currentConversationId;
    QString m_currentPeerName;
    QString m_pendingOpenConversationId;
    QString m_pendingImageConversationId;
    QString m_pendingImageCaption;
    QString m_localDisplayName;
    bool m_busy = false;
    bool m_uploading = false;
    bool m_realtimeSubscribed = false;
};

#endif // DIRECTMESSAGECONTROLLER_H
