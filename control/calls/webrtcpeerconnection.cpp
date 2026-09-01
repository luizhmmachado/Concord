#include "webrtcpeerconnection.h"

#include <QMetaObject>
#include <QDebug>

WebRtcPeerConnection::WebRtcPeerConnection(const QString &peerId, QObject *parent)
    : QObject(parent)
    , m_peerId(peerId)
{
}

WebRtcPeerConnection::~WebRtcPeerConnection()
{
    teardownPipeline();
}

void WebRtcPeerConnection::start(bool polite)
{
    m_polite = polite;
    buildPipeline();
}

void WebRtcPeerConnection::stop()
{
    teardownPipeline();
}

void WebRtcPeerConnection::buildPipeline()
{
    if (m_pipeline)
        return;

    GError *error = nullptr;

#ifdef Q_OS_WIN
    static const char *screenSrc = "d3d11screencapturesrc show-cursor=true";
#elif defined(Q_OS_MAC)
    static const char *screenSrc = "avfvideosrc capture-screen=true";
#else
    static const char *screenSrc = "ximagesrc use-damage=false";
#endif

    const QString description = QStringLiteral(
        "webrtcbin name=sendrecv bundle-policy=max-bundle stun-server=stun://stun.l.google.com:19302 "
        "autoaudiosrc ! queue ! audioconvert ! audioresample ! volume name=micvolume ! opusenc ! rtpopuspay pt=111 ! "
        "queue ! sendrecv. "
        "autovideosrc ! videoconvert ! valve name=cameravalve drop=true ! videoscale ! "
        "video/x-raw,width=640,height=480 ! vp8enc deadline=1 target-bitrate=1000000 ! rtpvp8pay pt=96 ! "
        "queue ! sendrecv. "
        "%1 ! videoconvert ! valve name=screenvalve drop=true ! videoscale ! "
        "vp8enc deadline=1 target-bitrate=2500000 ! rtpvp8pay pt=97 ! queue ! sendrecv."
    ).arg(screenSrc);

    m_pipeline = gst_parse_launch(description.toUtf8().constData(), &error);
    if (!m_pipeline || error) {
        emit errorOccurred(m_peerId, error ? QString::fromUtf8(error->message) : "Failed to build pipeline");
        if (error)
            g_error_free(error);
        return;
    }

    m_webrtcbin = gst_bin_get_by_name(GST_BIN(m_pipeline), "sendrecv");
    m_micVolume = gst_bin_get_by_name(GST_BIN(m_pipeline), "micvolume");
    m_cameraValve = gst_bin_get_by_name(GST_BIN(m_pipeline), "cameravalve");
    m_screenValve = gst_bin_get_by_name(GST_BIN(m_pipeline), "screenvalve");

    g_signal_connect(m_webrtcbin, "on-negotiation-needed", G_CALLBACK(onNegotiationNeeded), this);
    g_signal_connect(m_webrtcbin, "on-ice-candidate", G_CALLBACK(onIceCandidate), this);
    g_signal_connect(m_webrtcbin, "notify::ice-connection-state", G_CALLBACK(onIceConnectionStateNotify), this);

    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_set_sync_handler(m_bus, onBusMessage, this, nullptr);

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void WebRtcPeerConnection::teardownPipeline()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }
    // gst_object_unref is safe on nullptr-checked pointers only; guard each.
    if (m_micVolume) { gst_object_unref(m_micVolume); m_micVolume = nullptr; }
    if (m_cameraValve) { gst_object_unref(m_cameraValve); m_cameraValve = nullptr; }
    if (m_screenValve) { gst_object_unref(m_screenValve); m_screenValve = nullptr; }
    if (m_webrtcbin) { gst_object_unref(m_webrtcbin); m_webrtcbin = nullptr; }
    if (m_bus) {
        gst_bus_set_sync_handler(m_bus, nullptr, nullptr, nullptr);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }
    if (m_pipeline) { gst_object_unref(m_pipeline); m_pipeline = nullptr; }
}

void WebRtcPeerConnection::setMicMuted(bool muted)
{
    if (m_micVolume)
        g_object_set(m_micVolume, "mute", muted ? TRUE : FALSE, nullptr);
}

void WebRtcPeerConnection::setCameraEnabled(bool enabled)
{
    if (m_cameraValve)
        g_object_set(m_cameraValve, "drop", enabled ? FALSE : TRUE, nullptr);
}

void WebRtcPeerConnection::setScreenSharing(bool enabled)
{
    if (m_screenValve)
        g_object_set(m_screenValve, "drop", enabled ? FALSE : TRUE, nullptr);
}

void WebRtcPeerConnection::createOffer()
{
    GstPromise *promise = gst_promise_new_with_change_func(onOfferCreated, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-offer", nullptr, promise);
}

void WebRtcPeerConnection::createAnswer()
{
    GstPromise *promise = gst_promise_new_with_change_func(onAnswerCreated, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-answer", nullptr, promise);
}

void WebRtcPeerConnection::applyRemoteDescription(const QString &sdpType, const QString &sdp)
{
    if (!m_webrtcbin)
        return;

    GstSDPMessage *sdpMessage = nullptr;
    gst_sdp_message_new(&sdpMessage);
    gst_sdp_message_parse_buffer(reinterpret_cast<const guint8 *>(sdp.toUtf8().constData()), sdp.toUtf8().size(), sdpMessage);

    const GstWebRTCSDPType type = sdpType == "offer" ? GST_WEBRTC_SDP_TYPE_OFFER : GST_WEBRTC_SDP_TYPE_ANSWER;
    GstWebRTCSessionDescription *description = gst_webrtc_session_description_new(type, sdpMessage);

    GstPromise *promise = gst_promise_new_with_change_func([](GstPromise *promise, gpointer userData) {
        gst_promise_unref(promise);
        auto *self = static_cast<WebRtcPeerConnection *>(userData);
        if (self->m_polite == false && self->m_webrtcbin) {
            // We received an offer -> we must answer.
        }
    }, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "set-remote-description", description, promise);

    if (type == GST_WEBRTC_SDP_TYPE_OFFER)
        createAnswer();

    gst_webrtc_session_description_free(description);
}

void WebRtcPeerConnection::applyRemoteIceCandidate(const QString &candidate, int mLineIndex)
{
    if (!m_webrtcbin)
        return;
    g_signal_emit_by_name(m_webrtcbin, "add-ice-candidate", static_cast<guint>(mLineIndex), candidate.toUtf8().constData());
}

void WebRtcPeerConnection::onNegotiationNeeded(GstElement *, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    QMetaObject::invokeMethod(self, [self]() { self->createOffer(); }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onOfferCreated(GstPromise *promise, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *description = nullptr;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &description, nullptr);
    gst_promise_unref(promise);

    GstPromise *setLocalPromise = gst_promise_new();
    g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", description, setLocalPromise);
    gst_promise_interrupt(setLocalPromise);
    gst_promise_unref(setLocalPromise);

    gchar *sdpText = gst_sdp_message_as_text(description->sdp);
    const QString sdp = QString::fromUtf8(sdpText);
    g_free(sdpText);
    gst_webrtc_session_description_free(description);

    QMetaObject::invokeMethod(self, [self, sdp]() {
        emit self->localDescriptionReady(self->m_peerId, "offer", sdp);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onAnswerCreated(GstPromise *promise, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *description = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &description, nullptr);
    gst_promise_unref(promise);

    GstPromise *setLocalPromise = gst_promise_new();
    g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", description, setLocalPromise);
    gst_promise_interrupt(setLocalPromise);
    gst_promise_unref(setLocalPromise);

    gchar *sdpText = gst_sdp_message_as_text(description->sdp);
    const QString sdp = QString::fromUtf8(sdpText);
    g_free(sdpText);
    gst_webrtc_session_description_free(description);

    QMetaObject::invokeMethod(self, [self, sdp]() {
        emit self->localDescriptionReady(self->m_peerId, "answer", sdp);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onIceCandidate(GstElement *, guint mLineIndex, gchar *candidate, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const QString candidateStr = QString::fromUtf8(candidate);
    const int index = static_cast<int>(mLineIndex);
    QMetaObject::invokeMethod(self, [self, candidateStr, index]() {
        emit self->localIceCandidateGathered(self->m_peerId, candidateStr, index);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onIceConnectionStateNotify(GstElement *webrtcbin, GParamSpec *, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    GstWebRTCICEConnectionState state;
    g_object_get(webrtcbin, "ice-connection-state", &state, nullptr);

    QString stateName;
    switch (state) {
    case GST_WEBRTC_ICE_CONNECTION_STATE_NEW: stateName = "new"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CHECKING: stateName = "checking"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED: stateName = "connected"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED: stateName = "completed"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_FAILED: stateName = "failed"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_DISCONNECTED: stateName = "disconnected"; break;
    case GST_WEBRTC_ICE_CONNECTION_STATE_CLOSED: stateName = "closed"; break;
    default: stateName = "unknown"; break;
    }

    QMetaObject::invokeMethod(self, [self, stateName]() {
        emit self->connectionStateChanged(self->m_peerId, stateName);
    }, Qt::QueuedConnection);
}

GstBusSyncReply WebRtcPeerConnection::onBusMessage(GstBus *, GstMessage *message, gpointer userData)
{
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        const QString errorText = QString::fromUtf8(error->message);
        g_error_free(error);
        g_free(debug);
        QMetaObject::invokeMethod(self, [self, errorText]() {
            emit self->errorOccurred(self->m_peerId, errorText);
        }, Qt::QueuedConnection);
    }
    gst_message_unref(message);
    return GST_BUS_DROP;
}
