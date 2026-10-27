#include "callmediaengine.h"
#include "videostreamsource.h"

#include <QDebug>
#include <QMetaObject>

CallMediaEngine::CallMediaEngine(QObject *parent)
    : QObject(parent)
    , m_cameraPreview(new VideoStreamSource(this))
    , m_screenPreview(new VideoStreamSource(this))
{
    qInfo().noquote() << "CallMediaEngine::CallMediaEngine";
    m_cameraPreview->setObjectName(QStringLiteral("cameraPreview"));
    m_screenPreview->setObjectName(QStringLiteral("screenPreview"));
}

#if CONCORD_HAS_GSTREAMER

namespace {

const char *kStunServer = "stun://stun.l.google.com:19302";

constexpr int kScreenShare720p = 720;
constexpr int kScreenShare1080p = 1080;
constexpr int kScreenShare30Fps = 30;
constexpr int kScreenShare60Fps = 60;

int normalizeScreenShareHeight(int height)
{
    return height >= kScreenShare1080p ? kScreenShare1080p : kScreenShare720p;
}

int normalizeScreenShareFps(int fps)
{
    return fps >= kScreenShare60Fps ? kScreenShare60Fps : kScreenShare30Fps;
}

int screenShareBitrate(int height, int fps)
{
    if (height >= kScreenShare1080p)
        return fps >= kScreenShare60Fps ? 8000000 : 5000000;
    return fps >= kScreenShare60Fps ? 4000000 : 2500000;
}

#ifdef Q_OS_WIN
const char *kScreenSrc = "d3d11screencapturesrc show-cursor=true";
#elif defined(Q_OS_MAC)
const char *kScreenSrc = "avfvideosrc capture-screen=true";
#else
const char *kScreenSrc = "ximagesrc use-damage=false";
#endif

#ifdef Q_OS_WIN
const char *kCameraSrc = "ksvideosrc";
#elif defined(Q_OS_MAC)
const char *kCameraSrc = "avfvideosrc";
#else
const char *kCameraSrc = "v4l2src";
#endif

GstBusSyncReply busHandler(GstBus *, GstMessage *message, gpointer userData)
{
    auto *self = static_cast<CallMediaEngine *>(userData);
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_WARNING) {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_warning(message, &error, &debug);
        qWarning() << "[media] WARNING" << GST_OBJECT_NAME(message->src) << error->message << (debug ? debug : "");
        g_error_free(error);
        g_free(debug);
    } else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        qWarning() << "[media] ERROR" << GST_OBJECT_NAME(message->src) << error->message << (debug ? debug : "");
        const QString text = QStringLiteral("%1 (%2)")
                                 .arg(QString::fromUtf8(error->message),
                                      QString::fromUtf8(GST_OBJECT_NAME(message->src)));
        g_error_free(error);
        g_free(debug);
        QMetaObject::invokeMethod(self, [self, text]() { emit self->errorOccurred(text); }, Qt::QueuedConnection);
    }
    gst_message_unref(message);
    return GST_BUS_DROP;
}

} // namespace

CallMediaEngine::~CallMediaEngine()
{
    qInfo().noquote() << "CallMediaEngine::~CallMediaEngine";
    stop();
}

bool CallMediaEngine::isRunning() const
{
    qInfo().noquote() << "CallMediaEngine::isRunning";
    return m_pipeline != nullptr;
}

bool CallMediaEngine::start()
{
    qInfo().noquote() << "CallMediaEngine::start";
    if (m_pipeline)
        return true;

    const QString description = QStringLiteral(
        "videotestsrc name=camblack pattern=black is-live=true ! "
        "video/x-raw,width=640,height=480,framerate=15/1 ! videoconvert ! "
        "input-selector name=camsel sync-streams=false ! tee name=camtee allow-not-linked=true "
        "camtee. ! queue max-size-buffers=1 leaky=downstream ! videoconvert ! "
        "video/x-raw,format=BGRA ! appsink name=campreview "

        "videotestsrc name=scrblack pattern=black is-live=true ! "
        "video/x-raw,width=1280,height=720,framerate=15/1 ! videoconvert ! "
        "input-selector name=scrsel sync-streams=false ! tee name=scrtee allow-not-linked=true "
        "scrtee. ! valve name=scrpreviewvalve drop=true ! queue max-size-buffers=1 leaky=downstream ! "
        "videoconvert ! videorate drop-only=true ! video/x-raw,framerate=15/1 ! videoscale ! "
        "video/x-raw,height=720,pixel-aspect-ratio=1/1 ! videoconvert ! "
        "video/x-raw,format=BGRA ! appsink name=scrpreview "

        "autoaudiosrc ! queue ! audioconvert ! audioresample ! "
        "volume name=micvolume ! tee name=mictee allow-not-linked=true ");

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(description.toUtf8().constData(), &error);
    if (!m_pipeline || error) {
        const QString message = error ? QString::fromUtf8(error->message)
                                      : QStringLiteral("Falha ao construir a pipeline de captura");
        qWarning() << "[media] parse_launch falhou:" << message;
        if (error)
            g_error_free(error);
        emit errorOccurred(message);
        return false;
    }
    qInfo() << "[media] pipeline criada";

    m_cameraSelector = gst_bin_get_by_name(GST_BIN(m_pipeline), "camsel");
    m_screenSelector = gst_bin_get_by_name(GST_BIN(m_pipeline), "scrsel");
    m_micVolume = gst_bin_get_by_name(GST_BIN(m_pipeline), "micvolume");
    m_cameraTee = gst_bin_get_by_name(GST_BIN(m_pipeline), "camtee");
    m_screenTee = gst_bin_get_by_name(GST_BIN(m_pipeline), "scrtee");
    m_micTee = gst_bin_get_by_name(GST_BIN(m_pipeline), "mictee");
    m_screenPreviewValve = gst_bin_get_by_name(GST_BIN(m_pipeline), "scrpreviewvalve");
    if (m_screenPreviewValve)
        g_object_set(m_screenPreviewValve, "drop", m_screenPreviewEnabled ? FALSE : TRUE, nullptr);

    if (m_cameraSelector)
        m_cameraBlackPad = gst_element_get_static_pad(m_cameraSelector, "sink_0");
    if (m_screenSelector)
        m_screenBlackPad = gst_element_get_static_pad(m_screenSelector, "sink_0");

    qInfo() << "[media] elementos:" << "camsel" << (m_cameraSelector != nullptr)
            << "scrsel" << (m_screenSelector != nullptr)
            << "camtee" << (m_cameraTee != nullptr)
            << "scrtee" << (m_screenTee != nullptr)
            << "mictee" << (m_micTee != nullptr)
            << "scrpreviewvalve" << (m_screenPreviewValve != nullptr)
            << "camBlackPad" << (m_cameraBlackPad != nullptr)
            << "scrBlackPad" << (m_screenBlackPad != nullptr);

    if (GstElement *sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "campreview")) {
        attachAppSinkToVideoSource(sink, m_cameraPreview);
        gst_object_unref(sink);
    } else {
        qWarning() << "[media] appsink campreview nao encontrado";
    }
    if (GstElement *sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "scrpreview")) {
        attachAppSinkToVideoSource(sink, m_screenPreview);
        gst_object_unref(sink);
    } else {
        qWarning() << "[media] appsink scrpreview nao encontrado";
    }

    m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_set_sync_handler(m_bus, busHandler, this, nullptr);

    const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    qInfo() << "[media] set_state(PLAYING) ->" << gst_element_state_change_return_get_name(ret);

    setMicMuted(m_micMuted);
    return true;
}

void CallMediaEngine::stop()
{
    qInfo().noquote() << "CallMediaEngine::stop";
    if (!m_pipeline)
        return;

    for (auto &entry : m_peers)
        removePeerBin(entry.first);
    m_peers.clear();

    setCameraEnabled(false);
    setScreenSharingEnabled(false);

    gst_element_set_state(m_pipeline, GST_STATE_NULL);

    auto release = [](GstElement **e) { if (*e) { gst_object_unref(*e); *e = nullptr; } };
    auto releasePad = [](GstPad **p) { if (*p) { gst_object_unref(*p); *p = nullptr; } };

    release(&m_cameraSelector);
    release(&m_screenSelector);
    release(&m_micVolume);
    release(&m_cameraTee);
    release(&m_screenTee);
    release(&m_micTee);
    release(&m_screenPreviewValve);
    releasePad(&m_cameraBlackPad);
    releasePad(&m_screenBlackPad);

    if (m_bus) {
        gst_bus_set_sync_handler(m_bus, nullptr, nullptr, nullptr);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    gst_object_unref(m_pipeline);
    m_pipeline = nullptr;

    m_cameraPreview->clear();
    m_screenPreview->clear();
}

void CallMediaEngine::selectPad(GstElement *selector, GstPad *pad)
{
    if (selector && pad)
        g_object_set(selector, "active-pad", pad, nullptr);
}

GstElement *CallMediaEngine::addLiveSource(const QString &description, GstElement *selector, GstElement **)
{
    GError *error = nullptr;
    GstElement *bin = gst_parse_bin_from_description(description.toUtf8().constData(), TRUE, &error);
    if (!bin || error) {
        const QString message = error ? QString::fromUtf8(error->message)
                                      : QStringLiteral("Falha ao abrir o dispositivo de captura");
        qWarning() << "[media] parse_bin falhou:" << message;
        if (error)
            g_error_free(error);
        emit errorOccurred(message);
        return nullptr;
    }

    gst_bin_add(GST_BIN(m_pipeline), bin);

    GstPad *selectorPad = gst_element_get_request_pad(selector, "sink_%u");
    if (selectorPad && g_object_class_find_property(G_OBJECT_GET_CLASS(selectorPad), "always-ok"))
        g_object_set(selectorPad, "always-ok", TRUE, nullptr);
    GstPad *srcPad = gst_element_get_static_pad(bin, "src");
    const bool linked = srcPad && selectorPad && gst_pad_link(srcPad, selectorPad) == GST_PAD_LINK_OK;
    if (srcPad)
        gst_object_unref(srcPad);

    if (!linked) {
        qWarning() << "[media] falha ao ligar fonte ao input-selector";
        if (selectorPad) {
            gst_element_release_request_pad(selector, selectorPad);
            gst_object_unref(selectorPad);
        }
        gst_bin_remove(GST_BIN(m_pipeline), bin);
        emit errorOccurred(QStringLiteral("Não foi possível conectar a fonte de vídeo"));
        return nullptr;
    }

    // Select before starting the live source. Otherwise its first buffer can
    // reach an inactive input-selector pad, get GST_FLOW_NOT_LINKED, and stop
    // producing frames before we have a chance to switch to it.
    selectPad(selector, selectorPad);
    const gboolean synced = gst_element_sync_state_with_parent(bin);
    if (!synced) {
        qWarning() << "[media] falha ao iniciar a fonte de vídeo";
        selectPad(selector, selector == m_cameraSelector ? m_cameraBlackPad : m_screenBlackPad);
        gst_element_release_request_pad(selector, selectorPad);
        gst_object_unref(selectorPad);
        gst_element_set_state(bin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), bin);
        emit errorOccurred(QStringLiteral("Não foi possível iniciar a fonte de vídeo"));
        return nullptr;
    }

    if (selector == m_cameraSelector)
        m_cameraLivePad = selectorPad;
    else
        m_screenLivePad = selectorPad;

    return bin;
}

void CallMediaEngine::removeLiveSource(GstElement **source, GstElement *selector, GstPad **pad)
{
    if (!*source)
        return;

    // Point the selector back at the black filler before tearing the source down.
    selectPad(selector, selector == m_cameraSelector ? m_cameraBlackPad : m_screenBlackPad);

    // Unlink the source before stopping it. If its streaming thread is blocked
    // pushing into the now-inactive selector pad, stopping first would wait on
    // that thread forever and freeze the GUI.
    if (*pad) {
        gst_element_release_request_pad(selector, *pad);
        gst_object_unref(*pad);
        *pad = nullptr;
    }

    gst_element_set_state(*source, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(m_pipeline), *source);
    *source = nullptr;
}

void CallMediaEngine::setCameraEnabled(bool enabled)
{
    m_cameraEnabled = enabled;
    if (!m_pipeline)
        return;

    if (enabled) {
        if (m_cameraSource)
            return;
        const QString desc = QStringLiteral(
                                 "%1 ! queue max-size-buffers=1 leaky=downstream ! "
                                 "videoconvert ! videoscale ! videorate ! "
                                 "video/x-raw,width=640,height=480,framerate=15/1 ! videoconvert")
                                 .arg(QString::fromUtf8(kCameraSrc));
        m_cameraSource = addLiveSource(desc, m_cameraSelector, nullptr);
        if (m_cameraSource)
            qInfo() << "[media] Câmera ligada";
    } else {
        if (!m_cameraSource)
            return;
        removeLiveSource(&m_cameraSource, m_cameraSelector, &m_cameraLivePad);
        m_cameraPreview->clear();
        qInfo() << "[media] Câmera desligada";
    }
}

void CallMediaEngine::setScreenSharingEnabled(bool enabled)
{
    m_screenSharing = enabled;
    if (!m_pipeline)
        return;

    if (enabled) {
        if (m_screenSource)
            return;
        const QString capture = m_screenCaptureDescription.isEmpty()
                                    ? QString::fromUtf8(kScreenSrc)
                                    : m_screenCaptureDescription;
        // Only the height is pinned so videoscale keeps the source aspect ratio.
        const QString desc = QStringLiteral(
                                 "%1 ! queue max-size-buffers=1 leaky=downstream ! "
                                 "videoconvert ! videorate drop-only=true ! "
                                 "video/x-raw,framerate=%3/1 ! videoscale add-borders=false ! "
                                 "video/x-raw,height=%2,pixel-aspect-ratio=1/1 ! videoconvert")
                                 .arg(capture)
                                 .arg(m_screenShareHeight)
                                 .arg(m_screenShareFps);
        m_screenSource = addLiveSource(desc, m_screenSelector, nullptr);
    } else {
        removeLiveSource(&m_screenSource, m_screenSelector, &m_screenLivePad);
        m_screenPreview->clear();
    }
}

void CallMediaEngine::setMicMuted(bool muted)
{
    m_micMuted = muted;
    if (m_micVolume)
        g_object_set(m_micVolume, "mute", muted ? TRUE : FALSE, nullptr);
}

void CallMediaEngine::setScreenCaptureDescription(const QString &description)
{
    qInfo().noquote() << "CallMediaEngine::setScreenCaptureDescription" << "description=" << description;
    m_screenCaptureDescription = description;
}

void CallMediaEngine::setScreenPreviewEnabled(bool enabled)
{
    qInfo().noquote() << "CallMediaEngine::setScreenPreviewEnabled" << "enabled=" << enabled;
    m_screenPreviewEnabled = enabled;
    if (m_screenPreviewValve)
        g_object_set(m_screenPreviewValve, "drop", enabled ? FALSE : TRUE, nullptr);
}

void CallMediaEngine::setScreenShareSettings(int height, int fps)
{
    const int normalizedHeight = normalizeScreenShareHeight(height);
    const int normalizedFps = normalizeScreenShareFps(fps);
    if (normalizedHeight == m_screenShareHeight && normalizedFps == m_screenShareFps)
        return;

    qInfo().noquote() << "CallMediaEngine::setScreenShareSettings"
                      << "height=" << normalizedHeight << "fps=" << normalizedFps;
    m_screenShareHeight = normalizedHeight;
    m_screenShareFps = normalizedFps;
    applyScreenShareEncoding();

    if (m_screenSharing && m_screenSource) {
        setScreenSharingEnabled(false);
        setScreenSharingEnabled(true);
    }
}

void CallMediaEngine::applyScreenShareEncoding()
{
    qInfo().noquote() << "CallMediaEngine::applyScreenShareEncoding"
                      << "height=" << m_screenShareHeight << "fps=" << m_screenShareFps;
    const int bitrate = screenShareBitrate(m_screenShareHeight, m_screenShareFps);
    const int keyframeDistance = m_screenShareFps * 2;

    for (auto &entry : m_peers) {
        for (GstElement *bin : entry.second.bins) {
            GstElement *encoder = gst_bin_get_by_name(GST_BIN(bin), "screenenc");
            if (!encoder)
                continue;
            g_object_set(encoder,
                         "target-bitrate", bitrate,
                         "keyframe-max-dist", keyframeDistance,
                         nullptr);
            gst_object_unref(encoder);
        }
    }
}

GstElement *CallMediaEngine::attachBranch(GstElement *tee, const QString &description, PeerBranches &peer)
{
    qInfo().noquote() << "CallMediaEngine::attachBranch" << "description=" << description;
    if (!tee) {
        qWarning() << "[media] attachBranch: tee inexistente para" << description.left(40);
        return nullptr;
    }

    GError *error = nullptr;
    GstElement *bin = gst_parse_bin_from_description(description.toUtf8().constData(), TRUE, &error);
    if (!bin || error) {
        qWarning() << "[media] attachBranch: parse falhou:"
                   << (error ? error->message : "?") << "desc=" << description.left(60);
        if (error)
            g_error_free(error);
        return nullptr;
    }

    gst_bin_add(GST_BIN(m_pipeline), bin);

    GstPad *teePad = gst_element_get_request_pad(tee, "src_%u");
    GstPad *binSink = gst_element_get_static_pad(bin, "sink");
    const bool linked = teePad && binSink && gst_pad_link(teePad, binSink) == GST_PAD_LINK_OK;
    if (binSink)
        gst_object_unref(binSink);

    if (!linked) {
        qWarning() << "[media] attachBranch: falha ao ligar o tee" << GST_OBJECT_NAME(tee);
        if (teePad) {
            gst_element_release_request_pad(tee, teePad);
            gst_object_unref(teePad);
        }
        gst_bin_remove(GST_BIN(m_pipeline), bin);
        return nullptr;
    }

    GstPad *binSrc = gst_element_get_static_pad(bin, "src");
    GstPad *webrtcSink = gst_element_get_request_pad(peer.webrtcbin, "sink_%u");
    const bool sent = binSrc && webrtcSink && gst_pad_link(binSrc, webrtcSink) == GST_PAD_LINK_OK;
    if (binSrc)
        gst_object_unref(binSrc);
    if (webrtcSink)
        gst_object_unref(webrtcSink);

    if (!sent) {
        qWarning() << "[media] attachBranch: falha ao ligar no webrtcbin" << GST_OBJECT_NAME(tee);
        gst_element_release_request_pad(tee, teePad);
        gst_object_unref(teePad);
        gst_bin_remove(GST_BIN(m_pipeline), bin);
        return nullptr;
    }

    peer.bins.push_back(bin);
    peer.teePads.emplace_back(tee, teePad);
    return bin;
}

GstElement *CallMediaEngine::createPeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::createPeerBin" << "peerId=" << peerId;
    if (!m_pipeline || m_peers.count(peerId))
        return nullptr;

    GstElement *webrtcbin = gst_element_factory_make("webrtcbin", peerId.toUtf8().constData());
    if (!webrtcbin) {
        emit errorOccurred(QStringLiteral("webrtcbin indisponível (instale gstreamer1.0-plugins-bad)"));
        return nullptr;
    }

    g_object_set(webrtcbin, "bundle-policy", 3 /* max-bundle */, "stun-server", kStunServer, nullptr);
    gst_bin_add(GST_BIN(m_pipeline), webrtcbin);

    PeerBranches peer;
    peer.webrtcbin = webrtcbin;

    // Each branch must end in a real element: gst_parse only recognises a caps
    // filter when something follows it, otherwise it looks for an element named
    // "application" and the whole branch fails to build.
    attachBranch(m_micTee,
                 QStringLiteral("queue max-size-buffers=10 leaky=downstream ! audioconvert ! audioresample ! "
                                "opusenc ! rtpopuspay pt=111 ! "
                                "application/x-rtp,media=audio,encoding-name=OPUS,payload=111 ! queue"),
                 peer);

    attachBranch(m_cameraTee,
                 QStringLiteral("queue max-size-buffers=3 leaky=downstream ! videoconvert ! "
                                "vp8enc deadline=1 cpu-used=16 lag-in-frames=0 error-resilient=1 "
                                "keyframe-max-dist=30 target-bitrate=1000000 ! "
                                "rtpvp8pay pt=96 ! "
                                "application/x-rtp,media=video,encoding-name=VP8,payload=96 ! queue"),
                 peer);

    const int screenBitrate = screenShareBitrate(m_screenShareHeight, m_screenShareFps);
    attachBranch(m_screenTee,
                 QStringLiteral("queue max-size-buffers=3 leaky=downstream ! videoconvert ! "
                                "vp8enc name=screenenc deadline=1 cpu-used=16 lag-in-frames=0 "
                                "error-resilient=1 keyframe-max-dist=%1 target-bitrate=%2 ! "
                                "rtpvp8pay pt=97 ! "
                                "application/x-rtp,media=video,encoding-name=VP8,payload=97 ! queue")
                     .arg(m_screenShareFps * 2)
                     .arg(screenBitrate),
                 peer);

    // Deliberately left in NULL state: on-negotiation-needed would fire before
    // WebRtcPeerConnection connects its handlers. startPeerBin() resumes it.
    qInfo() << "[media] peer" << peerId << "branches anexadas =" << int(peer.bins.size()) << "de 3";
    m_peers.emplace(peerId, std::move(peer));
    return webrtcbin;
}

void CallMediaEngine::startPeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::startPeerBin" << "peerId=" << peerId;
    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return;

    for (GstElement *bin : it->second.bins)
        gst_element_sync_state_with_parent(bin);
    if (it->second.webrtcbin) {
        const gboolean ok = gst_element_sync_state_with_parent(it->second.webrtcbin);
        qInfo() << "[media] peer" << peerId << "webrtcbin start" << (ok ? "ok" : "falhou");
    }
}
void CallMediaEngine::removePeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::removePeerBin" << "peerId=" << peerId;
    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return;

    PeerBranches &peer = it->second;

    for (GstElement *bin : peer.bins) {
        gst_element_set_state(bin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), bin);
    }
    for (auto &pair : peer.teePads) {
        gst_element_release_request_pad(pair.first, pair.second);
        gst_object_unref(pair.second);
    }
    if (peer.webrtcbin) {
        gst_element_set_state(peer.webrtcbin, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline), peer.webrtcbin);
    }

    m_peers.erase(it);
}

#else // !CONCORD_HAS_GSTREAMER

CallMediaEngine::~CallMediaEngine() {
    qInfo().noquote() << "CallMediaEngine::~CallMediaEngine";
}

bool CallMediaEngine::start()
{
    qInfo().noquote() << "CallMediaEngine::start";
    emit errorOccurred(QStringLiteral("Compilado sem GStreamer: chamadas com áudio/vídeo desabilitadas."));
    return false;
}

void CallMediaEngine::stop()
{
    qInfo().noquote() << "CallMediaEngine::stop";
}
bool CallMediaEngine::isRunning() const
{
    qInfo().noquote() << "CallMediaEngine::isRunning";
    return false;
}
void CallMediaEngine::setCameraEnabled(bool enabled)
{
    qInfo().noquote() << "CallMediaEngine::setCameraEnabled" << "enabled=" << enabled;
    m_cameraEnabled = enabled;
}
void CallMediaEngine::setScreenSharingEnabled(bool enabled)
{
    qInfo().noquote() << "CallMediaEngine::setScreenSharingEnabled" << "enabled=" << enabled;
    m_screenSharing = enabled;
}
void CallMediaEngine::setMicMuted(bool muted)
{
    m_micMuted = muted;
}

void CallMediaEngine::setScreenCaptureDescription(const QString &description)
{
    qInfo().noquote() << "CallMediaEngine::setScreenCaptureDescription" << "description=" << description;
    m_screenCaptureDescription = description;
}

void CallMediaEngine::setScreenShareSettings(int height, int fps)
{
    qInfo().noquote() << "CallMediaEngine::setScreenShareSettings" << "height=" << height << "fps=" << fps;
    m_screenShareHeight = height >= 1080 ? 1080 : 720;
    m_screenShareFps = fps >= 60 ? 60 : 30;
}

void CallMediaEngine::setScreenPreviewEnabled(bool enabled)
{
    qInfo().noquote() << "CallMediaEngine::setScreenPreviewEnabled" << "enabled=" << enabled;
    m_screenPreviewEnabled = enabled;
}

void *CallMediaEngine::createPeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::createPeerBin" << "peerId=" << peerId;
    Q_UNUSED(peerId)
    return nullptr;
}

void CallMediaEngine::startPeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::startPeerBin" << "peerId=" << peerId;
    Q_UNUSED(peerId)
}

void CallMediaEngine::removePeerBin(const QString &peerId)
{
    qInfo().noquote() << "CallMediaEngine::removePeerBin" << "peerId=" << peerId;
    Q_UNUSED(peerId)
}

#endif // CONCORD_HAS_GSTREAMER
