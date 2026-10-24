#include "webrtcpeerconnection.h"
#include "videostreamsource.h"

#include <QMetaObject>
#include <QDebug>
#include <cstdio>

WebRtcPeerConnection::WebRtcPeerConnection(const QString &peerId, QObject *parent)
    : QObject(parent)
    , m_peerId(peerId)
    , m_remoteVideo(new VideoStreamSource(this))
    , m_remoteScreen(new VideoStreamSource(this))
{
    qInfo().noquote() << "WebRtcPeerConnection::WebRtcPeerConnection" << "peerId=" << peerId;
    m_remoteVideo->setObjectName(QStringLiteral("remote:%1").arg(peerId));
    m_remoteScreen->setObjectName(QStringLiteral("remoteScreen:%1").arg(peerId));
}

#if CONCORD_HAS_GSTREAMER

WebRtcPeerConnection::~WebRtcPeerConnection()
{
    qInfo().noquote() << "WebRtcPeerConnection::~WebRtcPeerConnection";
    stop();
}

void WebRtcPeerConnection::start(bool polite, GstElement *webrtcbin)
{
    qInfo().noquote() << "WebRtcPeerConnection::start" << "polite=" << polite << "webrtcbin=" << webrtcbin;
    if (!webrtcbin) {
        emit errorOccurred(m_peerId, QStringLiteral("webrtcbin não disponível"));
        return;
    }

    m_polite = polite;
    // Hold a reference: CallMediaEngine may drop the element from the pipeline
    // before this object is destroyed.
    m_webrtcbin = GST_ELEMENT(gst_object_ref(webrtcbin));

    g_signal_connect(m_webrtcbin, "on-negotiation-needed", G_CALLBACK(onNegotiationNeeded), this);
    g_signal_connect(m_webrtcbin, "on-ice-candidate", G_CALLBACK(onIceCandidate), this);
    g_signal_connect(m_webrtcbin, "notify::ice-connection-state", G_CALLBACK(onIceConnectionStateNotify), this);
    g_signal_connect(m_webrtcbin, "pad-added", G_CALLBACK(onIncomingStream), this);
}

void WebRtcPeerConnection::stop()
{
    qInfo().noquote() << "WebRtcPeerConnection::stop";
    if (m_webrtcbin) {
        g_signal_handlers_disconnect_by_data(m_webrtcbin, this);
        gst_object_unref(m_webrtcbin);
        m_webrtcbin = nullptr;
    }
    if (m_remoteVideo)
        m_remoteVideo->clear();
    if (m_remoteScreen)
        m_remoteScreen->clear();
}

void WebRtcPeerConnection::onIncomingStream(GstElement *webrtcbin, GstPad *pad, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onIncomingStream" << "webrtcbin=" << webrtcbin << "pad=" << pad << "userData=" << userData;
    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
        return;

    auto *self = static_cast<WebRtcPeerConnection *>(userData);

    GstElement *pipeline = GST_ELEMENT(gst_element_get_parent(webrtcbin));
    if (!pipeline)
        return;

    GstElement *decode = gst_element_factory_make("decodebin", nullptr);
    if (!decode) {
        gst_object_unref(pipeline);
        return;
    }

    g_signal_connect(decode, "pad-added", G_CALLBACK(onDecodedPad), self);
    // The m-line order matches how CallMediaEngine adds the branches:
    // 0 = microphone, 1 = camera, 2 = screen.
    int mline = 0;
    if (const gchar *name = GST_PAD_NAME(pad))
        sscanf(name, "src_%d", &mline);
    g_object_set_data(G_OBJECT(decode), "concord-mline", GINT_TO_POINTER(mline));
    qInfo() << "[rtc]" << self->m_peerId << "stream recebido pad=" << GST_PAD_NAME(pad) << "mline=" << mline;

    gst_bin_add(GST_BIN(pipeline), decode);
    gst_element_sync_state_with_parent(decode);

    GstPad *sink = gst_element_get_static_pad(decode, "sink");
    gst_pad_link(pad, sink);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
}

void WebRtcPeerConnection::onDecodedPad(GstElement *decodebin, GstPad *pad, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onDecodedPad" << "decodebin=" << decodebin << "pad=" << pad << "userData=" << userData;
    auto *self = static_cast<WebRtcPeerConnection *>(userData);

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, nullptr);
    if (!caps)
        return;

    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    const bool isVideo = g_str_has_prefix(name, "video/");
    const bool isAudio = g_str_has_prefix(name, "audio/");
    gst_caps_unref(caps);

    if (!isVideo && !isAudio)
        return;

    GstElement *pipeline = GST_ELEMENT(gst_element_get_parent(decodebin));
    if (!pipeline)
        return;

    const char *description = isVideo
        ? "queue max-size-buffers=2 leaky=downstream ! videoconvert ! video/x-raw,format=BGRA ! appsink name=remotesink"
        : "queue max-size-buffers=10 leaky=downstream ! audioconvert ! audioresample ! autoaudiosink sync=false";

    GError *error = nullptr;
    GstElement *bin = gst_parse_bin_from_description(description, TRUE, &error);
    if (!bin || error) {
        if (error)
            g_error_free(error);
        gst_object_unref(pipeline);
        return;
    }

    if (isVideo) {
        const int mline = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(decodebin), "concord-mline"));
        VideoStreamSource *target = mline >= 2 ? self->m_remoteScreen : self->m_remoteVideo;
        qInfo() << "[rtc]" << self->m_peerId << "video decodificado mline=" << mline
                << "->" << target->objectName();
        if (GstElement *sink = gst_bin_get_by_name(GST_BIN(bin), "remotesink")) {
            attachAppSinkToVideoSource(sink, target);
            gst_object_unref(sink);
        }
    }

    gst_bin_add(GST_BIN(pipeline), bin);
    gst_element_sync_state_with_parent(bin);

    GstPad *binSink = gst_element_get_static_pad(bin, "sink");
    gst_pad_link(pad, binSink);
    gst_object_unref(binSink);
    gst_object_unref(pipeline);
}

void WebRtcPeerConnection::createOffer()
{
    qInfo().noquote() << "WebRtcPeerConnection::createOffer";
    if (!m_webrtcbin)
        return;
    GstPromise *promise = gst_promise_new_with_change_func(onOfferCreated, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-offer", nullptr, promise);
}

void WebRtcPeerConnection::createAnswer()
{
    qInfo().noquote() << "WebRtcPeerConnection::createAnswer";
    if (!m_webrtcbin)
        return;
    GstPromise *promise = gst_promise_new_with_change_func(onAnswerCreated, this, nullptr);
    g_signal_emit_by_name(m_webrtcbin, "create-answer", nullptr, promise);
}

void WebRtcPeerConnection::applyRemoteDescription(const QString &sdpType, const QString &sdp)
{
    qInfo().noquote() << "WebRtcPeerConnection::applyRemoteDescription" << "sdpType=" << sdpType << "sdp=" << sdp;
    qInfo() << "[rtc]" << m_peerId << "remote description" << sdpType << "bytes=" << sdp.size()
            << "hasBin=" << (m_webrtcbin != nullptr);
    if (!m_webrtcbin)
        return;

    const QByteArray sdpBytes = sdp.toUtf8();
    GstSDPMessage *sdpMessage = nullptr;
    gst_sdp_message_new(&sdpMessage);
    gst_sdp_message_parse_buffer(reinterpret_cast<const guint8 *>(sdpBytes.constData()), sdpBytes.size(), sdpMessage);

    const GstWebRTCSDPType type = sdpType == "offer" ? GST_WEBRTC_SDP_TYPE_OFFER : GST_WEBRTC_SDP_TYPE_ANSWER;
    GstWebRTCSessionDescription *description = gst_webrtc_session_description_new(type, sdpMessage);

    GstPromise *promise = gst_promise_new();
    g_signal_emit_by_name(m_webrtcbin, "set-remote-description", description, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    if (type == GST_WEBRTC_SDP_TYPE_OFFER)
        createAnswer();

    gst_webrtc_session_description_free(description);
}

void WebRtcPeerConnection::applyRemoteIceCandidate(const QString &candidate, int mLineIndex)
{
    qInfo().noquote() << "WebRtcPeerConnection::applyRemoteIceCandidate" << "candidate=" << candidate << "mLineIndex=" << mLineIndex;
    if (!m_webrtcbin)
        return;
    g_signal_emit_by_name(m_webrtcbin, "add-ice-candidate", static_cast<guint>(mLineIndex), candidate.toUtf8().constData());
}

void WebRtcPeerConnection::onNegotiationNeeded(GstElement *, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onNegotiationNeeded" << "userData=" << userData;
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    qInfo() << "[rtc]" << self->m_peerId << "negotiation-needed polite=" << self->m_polite;
    // Only the impolite side offers; the polite side waits and answers,
    // otherwise both peers would offer at once (glare).
    if (self->m_polite)
        return;
    QMetaObject::invokeMethod(self, [self]() { self->createOffer(); }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onOfferCreated(GstPromise *promise, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onOfferCreated" << "promise=" << promise << "userData=" << userData;
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *description = nullptr;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &description, nullptr);
    gst_promise_unref(promise);

    if (!description)
        return;

    GstPromise *setLocalPromise = gst_promise_new();
    g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", description, setLocalPromise);
    gst_promise_interrupt(setLocalPromise);
    gst_promise_unref(setLocalPromise);

    gchar *sdpText = gst_sdp_message_as_text(description->sdp);
    const QString sdp = QString::fromUtf8(sdpText);
    g_free(sdpText);
    gst_webrtc_session_description_free(description);

    QMetaObject::invokeMethod(self, [self, sdp]() {
        qInfo() << "[rtc]" << self->m_peerId << "offer criada";
        emit self->localDescriptionReady(self->m_peerId, "offer", sdp);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onAnswerCreated(GstPromise *promise, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onAnswerCreated" << "promise=" << promise << "userData=" << userData;
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *description = nullptr;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &description, nullptr);
    gst_promise_unref(promise);

    if (!description)
        return;

    GstPromise *setLocalPromise = gst_promise_new();
    g_signal_emit_by_name(self->m_webrtcbin, "set-local-description", description, setLocalPromise);
    gst_promise_interrupt(setLocalPromise);
    gst_promise_unref(setLocalPromise);

    gchar *sdpText = gst_sdp_message_as_text(description->sdp);
    const QString sdp = QString::fromUtf8(sdpText);
    g_free(sdpText);
    gst_webrtc_session_description_free(description);

    QMetaObject::invokeMethod(self, [self, sdp]() {
        qInfo() << "[rtc]" << self->m_peerId << "answer criada";
        emit self->localDescriptionReady(self->m_peerId, "answer", sdp);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onIceCandidate(GstElement *, guint mLineIndex, gchar *candidate, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onIceCandidate" << "mLineIndex=" << mLineIndex << "candidate=" << candidate << "userData=" << userData;
    auto *self = static_cast<WebRtcPeerConnection *>(userData);
    const QString candidateStr = QString::fromUtf8(candidate);
    const int index = static_cast<int>(mLineIndex);
    QMetaObject::invokeMethod(self, [self, candidateStr, index]() {
        emit self->localIceCandidateGathered(self->m_peerId, candidateStr, index);
    }, Qt::QueuedConnection);
}

void WebRtcPeerConnection::onIceConnectionStateNotify(GstElement *webrtcbin, GParamSpec *, gpointer userData)
{
    qInfo().noquote() << "WebRtcPeerConnection::onIceConnectionStateNotify" << "webrtcbin=" << webrtcbin << "userData=" << userData;
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
        qInfo() << "[rtc]" << self->m_peerId << "ice-connection-state" << stateName;
        emit self->connectionStateChanged(self->m_peerId, stateName);
    }, Qt::QueuedConnection);
}

#else // !CONCORD_HAS_GSTREAMER

WebRtcPeerConnection::~WebRtcPeerConnection() {
    qInfo().noquote() << "WebRtcPeerConnection::~WebRtcPeerConnection";
}

void WebRtcPeerConnection::start(bool polite, void *)
{
    qInfo().noquote() << "WebRtcPeerConnection::start" << "polite=" << polite;
    Q_UNUSED(polite)
    emit connectionStateChanged(m_peerId, QStringLiteral("disabled"));
    emit errorOccurred(m_peerId,
                       QStringLiteral("Compilado sem GStreamer: áudio/vídeo indisponíveis."));
}

void WebRtcPeerConnection::stop() {
    qInfo().noquote() << "WebRtcPeerConnection::stop";
}

void WebRtcPeerConnection::applyRemoteDescription(const QString &sdpType, const QString &sdp)
{
    qInfo().noquote() << "WebRtcPeerConnection::applyRemoteDescription" << "sdpType=" << sdpType << "sdp=" << sdp;
    Q_UNUSED(sdpType)
    Q_UNUSED(sdp)
}

void WebRtcPeerConnection::applyRemoteIceCandidate(const QString &candidate, int mLineIndex)
{
    qInfo().noquote() << "WebRtcPeerConnection::applyRemoteIceCandidate" << "candidate=" << candidate << "mLineIndex=" << mLineIndex;
    Q_UNUSED(candidate)
    Q_UNUSED(mLineIndex)
}

#endif // CONCORD_HAS_GSTREAMER
