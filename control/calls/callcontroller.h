#ifndef CALLCONTROLLER_H
#define CALLCONTROLLER_H

#include <QObject>
#include <QMap>
#include <memory>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "webrtcpeerconnection.h"
#include "callparticipantmodel.h"

// Orchestrates an in-progress call: local mute/camera/screen-share state,
// one WebRtcPeerConnection per remote participant, and signaling exchange
// over a Supabase Realtime broadcast topic ("call:<callId>").
class CallController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool inCall READ inCall NOTIFY inCallChanged)
    Q_PROPERTY(bool micMuted READ micMuted NOTIFY micMutedChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled NOTIFY cameraEnabledChanged)
    Q_PROPERTY(bool screenSharing READ screenSharing NOTIFY screenSharingChanged)
    Q_PROPERTY(CallParticipantModel *participants READ participants CONSTANT)

public:
    CallController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);
    ~CallController() override;

    bool inCall() const { return m_inCall; }
    bool micMuted() const { return m_micMuted; }
    bool cameraEnabled() const { return m_cameraEnabled; }
    bool screenSharing() const { return m_screenSharing; }
    CallParticipantModel *participants() { return &m_participants; }

    // Starts (or joins) a call scoped to a voice channel or a private DM.
    Q_INVOKABLE void startChannelCall(const QString &channelId);
    Q_INVOKABLE void startPrivateCall(const QString &conversationId, const QString &peerUserId, const QString &peerDisplayName);
    Q_INVOKABLE void leaveCall();

    Q_INVOKABLE void toggleMicMuted();
    Q_INVOKABLE void toggleCamera();
    Q_INVOKABLE void toggleScreenShare();

signals:
    void inCallChanged();
    void micMutedChanged();
    void cameraEnabledChanged();
    void screenSharingChanged();
    void errorOccurred(const QString &message);

private:
    void joinSignalingTopic(const QString &topic);
    void addPeer(const QString &peerId, const QString &displayName, bool polite);
    void removeAllPeers();
    void applyLocalStateToAllPeers();

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    CallParticipantModel m_participants;
    QMap<QString, std::unique_ptr<WebRtcPeerConnection>> m_peers;

    QString m_callId;
    QString m_signalingTopic;
    bool m_inCall = false;
    bool m_micMuted = false;
    bool m_cameraEnabled = false;
    bool m_screenSharing = false;
};

#endif // CALLCONTROLLER_H
