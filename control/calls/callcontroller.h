#ifndef CALLCONTROLLER_H
#define CALLCONTROLLER_H

#include <QObject>
#include <QHash>
#include <map>
#include <memory>
#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "webrtcpeerconnection.h"
#include "callparticipantmodel.h"
#include "callmediaengine.h"
#include "videostreamsource.h"
#include "screensharesourcemodel.h"

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
    Q_PROPERTY(int screenShareHeight READ screenShareHeight WRITE setScreenShareHeight NOTIFY screenShareSettingsChanged)
    Q_PROPERTY(int screenShareFps READ screenShareFps WRITE setScreenShareFps NOTIFY screenShareSettingsChanged)
    Q_PROPERTY(QString callChannelId READ callChannelId NOTIFY callChannelIdChanged)
    Q_PROPERTY(CallParticipantModel *participants READ participants CONSTANT)
    Q_PROPERTY(QObject *screenSource READ screenSource CONSTANT)
    // Whoever is sharing right now (local preview or a peer's incoming track).
    Q_PROPERTY(QObject *activeScreenSource READ activeScreenSource NOTIFY activeScreenChanged)
    Q_PROPERTY(bool anyoneScreenSharing READ anyoneScreenSharing NOTIFY activeScreenChanged)
    Q_PROPERTY(QString activeScreenOwner READ activeScreenOwner NOTIFY activeScreenChanged)
    Q_PROPERTY(ScreenShareSourceModel *screenShareSources READ screenShareSources CONSTANT)
    Q_PROPERTY(QString localDisplayName READ localDisplayName WRITE setLocalDisplayName NOTIFY localDisplayNameChanged)

public:
    CallController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent = nullptr);
    ~CallController() override;

    bool inCall() const { return m_inCall; }
    bool micMuted() const { return m_micMuted; }
    bool cameraEnabled() const { return m_cameraEnabled; }
    bool screenSharing() const { return m_screenSharing; }
    int screenShareHeight() const { return m_screenShareHeight; }
    int screenShareFps() const { return m_screenShareFps; }
    void setScreenShareHeight(int height);
    void setScreenShareFps(int fps);
    QString callChannelId() const { return m_callChannelId; }
    // Server that owns the voice channel; used to publish ephemeral presence.
    QString callServerId() const { return m_callServerId; }
    CallParticipantModel *participants() { return &m_participants; }
    QObject *screenSource() { return m_media.screenPreview(); }
    QObject *activeScreenSource();
    bool anyoneScreenSharing() const { return !m_activeScreenOwner.isEmpty(); }
    QString activeScreenOwner() const { return m_activeScreenOwner; }
    ScreenShareSourceModel *screenShareSources() { return &m_screenShareSources; }
    QString localDisplayName() const { return m_localDisplayName.isEmpty() ? QStringLiteral("Voce") : m_localDisplayName; }
    QString localUserId() const { return m_client->currentUserId(); }
    void setLocalDisplayName(const QString &name);

    // Starts (or joins) a call scoped to a voice channel or a private DM.
    // `serverId` is only needed for channel calls (presence is per server).
    Q_INVOKABLE void startChannelCall(const QString &channelId, const QString &serverId = QString());
    Q_INVOKABLE void startPrivateCall(const QString &conversationId, const QString &peerUserId, const QString &peerDisplayName);
    Q_INVOKABLE void leaveCall();
    // Leaves the signaling room/presence before the event loop stops.
    void shutdown();

    Q_INVOKABLE void toggleMicMuted();
    Q_INVOKABLE void toggleCamera();
    Q_INVOKABLE void toggleScreenShare();
    // Starts sharing the entry at `sourceIndex` of `screenShareSources`.
    Q_INVOKABLE void startScreenShare(int sourceIndex);
    Q_INVOKABLE void stopScreenShare();

signals:
    void inCallChanged();
    void micMutedChanged();
    void cameraEnabledChanged();
    void screenSharingChanged();
    void screenShareSettingsChanged();
    void callChannelIdChanged();
    void localDisplayNameChanged();
    void activeScreenChanged();
    void errorOccurred(const QString &message);

private:
    void joinSignalingTopic(const QString &topic);
    void announcePresence();
    void updateActiveScreen();
    // Returns the peer, creating it on demand when signaling arrives before the
    // `join` announcement.
    WebRtcPeerConnection *ensurePeer(const QString &peerId, const QString &displayName, bool polite);
    void addPeer(const QString &peerId, const QString &displayName, bool polite);
    void removeAllPeers();
    void applyLocalStateToAllPeers();
    void updateLocalParticipantState();
    void broadcastLocalState();
    void persistScreenShareSettings();

    SupabaseClient *m_client;
    RealtimeClient *m_realtime;
    CallParticipantModel m_participants;
    CallMediaEngine m_media;
    ScreenShareSourceModel m_screenShareSources;
    std::map<QString, std::unique_ptr<WebRtcPeerConnection>> m_peers;

    QString m_callChannelId;
    QString m_callServerId;
    QString m_signalingTopic;
    bool m_inCall = false;
    bool m_announced = false;
    bool m_micMuted = false;
    bool m_cameraEnabled = false;
    bool m_screenSharing = false;
    int m_screenShareHeight = 1080;
    int m_screenShareFps = 30;
    QString m_localParticipantId;
    QString m_localDisplayName;
    QString m_activeScreenOwner;
    QHash<QString, QString> m_knownNames;
};

#endif // CALLCONTROLLER_H
