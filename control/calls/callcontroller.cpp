#include "callcontroller.h"

#include <QUuid>
#include <QJsonDocument>
#include <QSettings>
#include <QDebug>

CallController::CallController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    qInfo().noquote() << "CallController::CallController" << "client=" << client << "realtime=" << realtime;
    connect(&m_media, &CallMediaEngine::errorOccurred, this, &CallController::errorOccurred);

    QSettings settings;
    settings.beginGroup(QStringLiteral("calls"));
    m_media.setScreenShareSettings(settings.value(QStringLiteral("screenShareHeight"), 1080).toInt(),
                                   settings.value(QStringLiteral("screenShareFps"), 30).toInt());
    settings.endGroup();
    m_screenShareHeight = m_media.screenShareHeight();
    m_screenShareFps = m_media.screenShareFps();

    connect(m_realtime, &RealtimeClient::topicJoined, this, [this](const QString &topic) {
        if (topic == m_signalingTopic)
            announcePresence();
    });

    connect(m_realtime, &RealtimeClient::broadcastReceived, this, [this](const QString &topic, const QString &event, const QJsonObject &payload) {
        if (topic != m_signalingTopic)
            return;

        const QString fromUserId = payload.value("from").toString();
        const QString toUserId = payload.value("to").toString();
        if (!toUserId.isEmpty() && toUserId != m_client->currentUserId())
            return;

        if (fromUserId.isEmpty() || fromUserId == m_client->currentUserId())
            return;

        if (event == "join") {
            qInfo() << "[call] join de" << fromUserId;
            // The newcomer announced itself: we are the impolite side and drive
            // the offer.
            ensurePeer(fromUserId, payload.value("displayName").toString(fromUserId), /*polite=*/false);
            broadcastLocalState();
        } else if (event == "sdp") {
            qInfo() << "[call] sdp" << payload.value("sdpType").toString() << "de" << fromUserId;
            // Signaling can arrive before `join` is processed, so create the peer
            // on demand; whoever receives the offer answers it.
            WebRtcPeerConnection *peer = ensurePeer(fromUserId, fromUserId, /*polite=*/true);
            if (peer)
                peer->applyRemoteDescription(payload.value("sdpType").toString(), payload.value("sdp").toString());
        } else if (event == "ice") {
            WebRtcPeerConnection *peer = ensurePeer(fromUserId, fromUserId, /*polite=*/true);
            if (peer)
                peer->applyRemoteIceCandidate(payload.value("candidate").toString(), payload.value("mLineIndex").toInt());
        } else if (event == "state") {
            const QString name = payload.value("displayName").toString();
            if (!name.isEmpty()) {
                // May arrive before the participant row exists (signaling order).
                m_knownNames.insert(fromUserId, name);
                m_participants.setDisplayName(fromUserId, name);
            }
            m_participants.setMuted(fromUserId, payload.value("micMuted").toBool(false));
            m_participants.setCameraOn(fromUserId, payload.value("cameraEnabled").toBool(false));
            m_participants.setScreenSharing(fromUserId, payload.value("screenSharing").toBool(false));
            updateActiveScreen();
        } else if (event == "leave") {
            if (auto it = m_peers.find(fromUserId); it != m_peers.end())
                it->second->stop();
            m_media.removePeerBin(fromUserId);
            m_peers.erase(fromUserId);
            m_participants.removeParticipant(fromUserId);
        }
    });
}

CallController::~CallController()
{
    qInfo().noquote() << "CallController::~CallController";
    leaveCall();
}

void CallController::shutdown()
{
    qInfo().noquote() << "CallController::shutdown";
    if (!m_inCall)
        return;

    // leaveCall() sends the signaling "leave" and the presence untrack; even
    // if the frame is lost the realtime server removes the session as soon as
    // the websocket closes.
    leaveCall();
}

void CallController::joinSignalingTopic(const QString &topic)
{
    qInfo().noquote() << "CallController::joinSignalingTopic" << "topic=" << topic;
    m_signalingTopic = topic;
    m_announced = false;
    m_realtime->joinTopic(topic);
    m_media.start();
    m_inCall = true;
    emit inCallChanged();

    m_localParticipantId = m_client->currentUserId();
    m_participants.addParticipant(m_localParticipantId, localDisplayName());
    m_participants.setConnectionState(m_localParticipantId, QStringLiteral("connected"));
    m_participants.setVideoSource(m_localParticipantId, m_media.cameraPreview());
    m_participants.setScreenSource(m_localParticipantId, m_media.screenPreview());
    updateLocalParticipantState();

    // The announcement is deferred to announcePresence(): broadcasts sent
    // before the server acknowledges the join are silently dropped.
}

void CallController::announcePresence()
{
    qInfo().noquote() << "CallController::announcePresence";
    if (m_announced || m_signalingTopic.isEmpty())
        return;
    m_announced = true;

    m_realtime->broadcast(m_signalingTopic, "join",
                          QJsonObject{{"from", m_client->currentUserId()},
                                      {"displayName", localDisplayName()}});
    broadcastLocalState();
}

void CallController::setLocalDisplayName(const QString &name)
{
    qInfo().noquote() << "CallController::setLocalDisplayName" << "name=" << name;
    if (m_localDisplayName == name)
        return;
    m_localDisplayName = name;
    if (!m_localParticipantId.isEmpty())
        m_participants.setDisplayName(m_localParticipantId, localDisplayName());
    emit localDisplayNameChanged();
}

void CallController::startChannelCall(const QString &channelId, const QString &serverId)
{
    qInfo().noquote() << "CallController::startChannelCall" << "channelId=" << channelId
                      << "serverId=" << serverId;
    if (m_inCall)
        leaveCall();
    m_callChannelId = channelId;
    m_callServerId = serverId;
    emit callChannelIdChanged();
    // Everyone in the same voice channel must land on the same signaling topic,
    // so it is derived from the channel id. Supabase Realtime requires the
    // "realtime:" prefix; without it the join is rejected.
    joinSignalingTopic(QStringLiteral("realtime:call:channel:%1").arg(channelId));
    // Occupancy is published through Realtime Presence by
    // CallPresenceController, driven by callController's state signals.
}

void CallController::startPrivateCall(const QString &conversationId, const QString &peerUserId, const QString &peerDisplayName)
{
    qInfo().noquote() << "CallController::startPrivateCall" << "conversationId=" << conversationId << "peerUserId=" << peerUserId << "peerDisplayName=" << peerDisplayName;
    Q_UNUSED(peerUserId)
    Q_UNUSED(peerDisplayName)
    if (m_inCall)
        leaveCall();
    joinSignalingTopic(QStringLiteral("realtime:call:conversation:%1").arg(conversationId));
}

WebRtcPeerConnection *CallController::ensurePeer(const QString &peerId, const QString &displayName, bool polite)
{
    // A UUID here means the caller had no name yet; prefer one seen earlier.
    const QString name = displayName == peerId ? m_knownNames.value(peerId, peerId) : displayName;
    if (name != peerId)
        m_knownNames.insert(peerId, name);

    if (auto it = m_peers.find(peerId); it != m_peers.end()) {
        if (name != peerId)
            m_participants.setDisplayName(peerId, name);
        return it->second.get();
    }
    addPeer(peerId, name, polite);
    auto it = m_peers.find(peerId);
    return it != m_peers.end() ? it->second.get() : nullptr;
}

void CallController::addPeer(const QString &peerId, const QString &displayName, bool polite)
{
    qInfo().noquote() << "CallController::addPeer" << "peerId=" << peerId << "displayName=" << displayName << "polite=" << polite;
    if (m_peers.find(peerId) != m_peers.end())
        return;

    // Lifetime is owned exclusively by m_peers (unique_ptr); do NOT also
    // parent it to `this`, or Qt's parent/child deletion would race with
    // the unique_ptr destructor and cause a double free.
    auto peer = std::make_unique<WebRtcPeerConnection>(peerId, nullptr);
    connect(peer.get(), &WebRtcPeerConnection::localDescriptionReady, this, [this](const QString &peerId, const QString &sdpType, const QString &sdp) {
        m_realtime->broadcast(m_signalingTopic, "sdp",
                               QJsonObject{{"from", m_client->currentUserId()}, {"to", peerId}, {"sdpType", sdpType}, {"sdp", sdp}});
    });
    connect(peer.get(), &WebRtcPeerConnection::localIceCandidateGathered, this, [this](const QString &peerId, const QString &candidate, int mLineIndex) {
        m_realtime->broadcast(m_signalingTopic, "ice",
                               QJsonObject{{"from", m_client->currentUserId()}, {"to", peerId}, {"candidate", candidate}, {"mLineIndex", mLineIndex}});
    });
    connect(peer.get(), &WebRtcPeerConnection::connectionStateChanged, this, [this](const QString &peerId, const QString &state) {
        m_participants.setConnectionState(peerId, state);
    });
    connect(peer.get(), &WebRtcPeerConnection::errorOccurred, this, [this](const QString &, const QString &message) {
        emit errorOccurred(message);
    });

    // The bin must only start once the peer's signal handlers are in place.
    peer->start(polite, m_media.createPeerBin(peerId));
    m_media.startPeerBin(peerId);

    m_participants.addParticipant(peerId, displayName);
    m_participants.setVideoSource(peerId, peer->remoteVideo());
    m_participants.setScreenSource(peerId, peer->remoteScreen());
    // std::map::operator[] default-constructs (unique_ptr -> nullptr) then
    // move-assigns here, unlike QMap which requires copy-assignment even
    // through operator[] and can't hold move-only types.
    m_peers[peerId] = std::move(peer);
}

void CallController::removeAllPeers()
{
    qInfo().noquote() << "CallController::removeAllPeers";
    // Disconnect the signal handlers before the engine frees each webrtcbin.
    for (auto &entry : m_peers)
        entry.second->stop();
    for (auto &entry : m_peers)
        m_media.removePeerBin(entry.first);
    m_peers.clear();
    m_participants.clear();
}

void CallController::leaveCall()
{
    qInfo().noquote() << "CallController::leaveCall";
    if (!m_inCall)
        return;

    if (!m_signalingTopic.isEmpty()) {
        m_realtime->broadcast(m_signalingTopic, "leave", QJsonObject{{"from", m_client->currentUserId()}});
        m_realtime->leaveTopic(m_signalingTopic);
    }

    removeAllPeers();
    m_participants.clear();
    m_media.stop();
    m_cameraEnabled = false;
    emit cameraEnabledChanged();
    m_screenSharing = false;
    emit screenSharingChanged();
    updateActiveScreen();
    m_localParticipantId.clear();
    m_signalingTopic.clear();
    m_callChannelId.clear();
    m_callServerId.clear();
    emit callChannelIdChanged();
    m_inCall = false;
    emit inCallChanged();
}

QObject *CallController::activeScreenSource()
{
    if (m_activeScreenOwner.isEmpty())
        return nullptr;
    if (m_activeScreenOwner == m_localParticipantId)
        return m_media.screenPreview();
    if (auto it = m_peers.find(m_activeScreenOwner); it != m_peers.end())
        return it->second->remoteScreen();
    return nullptr;
}

void CallController::updateActiveScreen()
{
    qInfo().noquote() << "CallController::updateActiveScreen";
    QString owner;
    if (m_screenSharing) {
        owner = m_localParticipantId;
    } else {
        for (const auto &entry : m_peers) {
            if (m_participants.isScreenSharing(entry.first)) {
                owner = entry.first;
                break;
            }
        }
    }

    if (owner == m_activeScreenOwner)
        return;
    m_activeScreenOwner = owner;
    emit activeScreenChanged();
}

void CallController::applyLocalStateToAllPeers()
{
    qInfo().noquote() << "CallController::applyLocalStateToAllPeers";
    m_media.setMicMuted(m_micMuted);
    m_media.setCameraEnabled(m_cameraEnabled);
    m_media.setScreenSharingEnabled(m_screenSharing);
    updateLocalParticipantState();
    updateActiveScreen();
    // CallPresenceController mirrors these flags to the server presence topic
    // through the signals emitted by the toggle methods.
}

void CallController::updateLocalParticipantState()
{
    qInfo().noquote() << "CallController::updateLocalParticipantState";
    if (m_localParticipantId.isEmpty())
        return;
    m_participants.setMuted(m_localParticipantId, m_micMuted);
    m_participants.setCameraOn(m_localParticipantId, m_cameraEnabled);
    m_participants.setScreenSharing(m_localParticipantId, m_screenSharing);
}

void CallController::broadcastLocalState()
{
    qInfo().noquote() << "CallController::broadcastLocalState";
    if (m_signalingTopic.isEmpty())
        return;
    m_realtime->broadcast(m_signalingTopic, "state",
                          QJsonObject{{"from", m_client->currentUserId()},
                                      {"displayName", localDisplayName()},
                                      {"micMuted", m_micMuted},
                                      {"cameraEnabled", m_cameraEnabled},
                                      {"screenSharing", m_screenSharing}});
}

void CallController::toggleMicMuted()
{
    qInfo().noquote() << "CallController::toggleMicMuted";
    m_micMuted = !m_micMuted;
    applyLocalStateToAllPeers();
    broadcastLocalState();
    emit micMutedChanged();
}

void CallController::toggleCamera()
{
    qInfo().noquote() << "CallController::toggleCamera";
    m_cameraEnabled = !m_cameraEnabled;
    applyLocalStateToAllPeers();
    broadcastLocalState();
    emit cameraEnabledChanged();
}

void CallController::setScreenShareHeight(int height)
{
    qInfo().noquote() << "CallController::setScreenShareHeight" << "height=" << height;
    const int normalized = height >= 1080 ? 1080 : 720;
    if (normalized == m_screenShareHeight)
        return;
    m_screenShareHeight = normalized;
    m_media.setScreenShareSettings(m_screenShareHeight, m_screenShareFps);
    m_screenShareHeight = m_media.screenShareHeight();
    persistScreenShareSettings();
    emit screenShareSettingsChanged();
}

void CallController::setScreenShareFps(int fps)
{
    qInfo().noquote() << "CallController::setScreenShareFps" << "fps=" << fps;
    const int normalized = fps >= 60 ? 60 : 30;
    if (normalized == m_screenShareFps)
        return;
    m_screenShareFps = normalized;
    m_media.setScreenShareSettings(m_screenShareHeight, m_screenShareFps);
    m_screenShareFps = m_media.screenShareFps();
    persistScreenShareSettings();
    emit screenShareSettingsChanged();
}

void CallController::setScreenPreviewEnabled(bool enabled)
{
    qInfo().noquote() << "CallController::setScreenPreviewEnabled" << "enabled=" << enabled;
    if (m_screenPreviewEnabled == enabled)
        return;
    m_screenPreviewEnabled = enabled;
    m_media.setScreenPreviewEnabled(enabled);
    emit screenPreviewEnabledChanged();
}

void CallController::persistScreenShareSettings()
{
    qInfo().noquote() << "CallController::persistScreenShareSettings"
                      << "height=" << m_screenShareHeight << "fps=" << m_screenShareFps;
    QSettings settings;
    settings.beginGroup(QStringLiteral("calls"));
    settings.setValue(QStringLiteral("screenShareHeight"), m_screenShareHeight);
    settings.setValue(QStringLiteral("screenShareFps"), m_screenShareFps);
    settings.endGroup();
}

void CallController::toggleScreenShare()
{
    qInfo().noquote() << "CallController::toggleScreenShare";
    if (m_screenSharing)
        stopScreenShare();
    else
        startScreenShare(0);
}

void CallController::startScreenShare(int sourceIndex)
{
    qInfo().noquote() << "CallController::startScreenShare" << "sourceIndex=" << sourceIndex;
    if (!m_screenShareSources.isValidIndex(sourceIndex)) {
        emit errorOccurred(QStringLiteral("Fonte de compartilhamento inválida"));
        return;
    }

    const ScreenShareSource &source = m_screenShareSources.at(sourceIndex);
    QString capture;
#ifdef Q_OS_LINUX
    if (source.kind == ScreenShareSource::Window) {
        capture = QStringLiteral("ximagesrc use-damage=true xid=%1").arg(source.windowId);
    } else {
        capture = QStringLiteral("ximagesrc use-damage=true startx=%1 starty=%2 endx=%3 endy=%4")
                      .arg(source.x)
                      .arg(source.y)
                      .arg(source.x + source.width - 1)
                      .arg(source.y + source.height - 1);
    }
#endif
    m_media.setScreenShareSettings(m_screenShareHeight, m_screenShareFps);
    m_media.setScreenCaptureDescription(capture);

    m_screenSharing = true;
    applyLocalStateToAllPeers();
    broadcastLocalState();
    emit screenSharingChanged();
}

void CallController::stopScreenShare()
{
    qInfo().noquote() << "CallController::stopScreenShare";
    if (!m_screenSharing)
        return;
    m_screenSharing = false;
    applyLocalStateToAllPeers();
    broadcastLocalState();
    emit screenSharingChanged();
}
