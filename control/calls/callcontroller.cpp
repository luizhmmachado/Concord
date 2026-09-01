#include "callcontroller.h"

#include <QUuid>
#include <QJsonDocument>

CallController::CallController(SupabaseClient *client, RealtimeClient *realtime, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_realtime(realtime)
{
    connect(m_client, &SupabaseClient::insertFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &error) {
        if (tag != "calls:create" || !ok || rows.isEmpty()) {
            if (tag == "calls:create")
                emit errorOccurred(error);
            return;
        }
        m_callId = rows.first().toObject().value("id").toString();
        joinSignalingTopic(QStringLiteral("call:%1").arg(m_callId));
    });

    connect(m_realtime, &RealtimeClient::broadcastReceived, this, [this](const QString &topic, const QString &event, const QJsonObject &payload) {
        if (topic != m_signalingTopic)
            return;

        const QString fromUserId = payload.value("from").toString();
        if (fromUserId.isEmpty() || fromUserId == m_client->currentUserId())
            return;

        if (event == "join") {
            addPeer(fromUserId, payload.value("displayName").toString(fromUserId), /*polite=*/true);
        } else if (event == "sdp") {
            if (auto it = m_peers.find(fromUserId); it != m_peers.end())
                it->get()->applyRemoteDescription(payload.value("sdpType").toString(), payload.value("sdp").toString());
        } else if (event == "ice") {
            if (auto it = m_peers.find(fromUserId); it != m_peers.end())
                it->get()->applyRemoteIceCandidate(payload.value("candidate").toString(), payload.value("mLineIndex").toInt());
        } else if (event == "leave") {
            m_peers.remove(fromUserId);
            m_participants.removeParticipant(fromUserId);
        }
    });
}

CallController::~CallController()
{
    leaveCall();
}

void CallController::joinSignalingTopic(const QString &topic)
{
    m_signalingTopic = topic;
    m_realtime->joinTopic(topic);
    m_inCall = true;
    emit inCallChanged();

    // Announce our presence so existing participants create a peer connection to us.
    m_realtime->broadcast(topic, "join", QJsonObject{{"from", m_client->currentUserId()}});
}

void CallController::startChannelCall(const QString &channelId)
{
    if (m_inCall)
        leaveCall();
    m_client->insert("calls", QJsonObject{{"channel_id", channelId}, {"initiator_id", m_client->currentUserId()}}, "calls:create");
}

void CallController::startPrivateCall(const QString &conversationId, const QString &peerUserId, const QString &peerDisplayName)
{
    Q_UNUSED(peerUserId)
    Q_UNUSED(peerDisplayName)
    if (m_inCall)
        leaveCall();
    m_client->insert("calls", QJsonObject{{"conversation_id", conversationId}, {"initiator_id", m_client->currentUserId()}}, "calls:create");
}

void CallController::addPeer(const QString &peerId, const QString &displayName, bool polite)
{
    if (m_peers.contains(peerId))
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

    peer->start(polite);
    peer->setMicMuted(m_micMuted);
    peer->setCameraEnabled(m_cameraEnabled);
    peer->setScreenSharing(m_screenSharing);

    m_participants.addParticipant(peerId, displayName);
    m_peers.insert(peerId, std::move(peer));
}

void CallController::removeAllPeers()
{
    m_peers.clear(); // unique_ptr destructors stop each pipeline and free GStreamer resources
    m_participants.clear();
}

void CallController::leaveCall()
{
    if (!m_inCall)
        return;

    if (!m_signalingTopic.isEmpty()) {
        m_realtime->broadcast(m_signalingTopic, "leave", QJsonObject{{"from", m_client->currentUserId()}});
        m_realtime->leaveTopic(m_signalingTopic);
    }

    removeAllPeers();
    m_signalingTopic.clear();
    m_callId.clear();
    m_inCall = false;
    emit inCallChanged();
}

void CallController::applyLocalStateToAllPeers()
{
    for (auto &peer : m_peers) {
        peer->setMicMuted(m_micMuted);
        peer->setCameraEnabled(m_cameraEnabled);
        peer->setScreenSharing(m_screenSharing);
    }
}

void CallController::toggleMicMuted()
{
    m_micMuted = !m_micMuted;
    applyLocalStateToAllPeers();
    emit micMutedChanged();
}

void CallController::toggleCamera()
{
    m_cameraEnabled = !m_cameraEnabled;
    applyLocalStateToAllPeers();
    emit cameraEnabledChanged();
}

void CallController::toggleScreenShare()
{
    m_screenSharing = !m_screenSharing;
    applyLocalStateToAllPeers();
    emit screenSharingChanged();
}
