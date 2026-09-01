#ifndef WEBRTCPEERCONNECTION_H
#define WEBRTCPEERCONNECTION_H

#include <QObject>
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>

// Wraps a single GStreamer `webrtcbin` peer connection (one per remote
// participant). Qt 5.15 has no native WebRTC stack, so real audio/video/
// screen-share transport is delegated to GStreamer (requires
// gstreamer1.0-plugins-{good,bad,ugly} and gstreamer1.0-nice at runtime).
//
// NOTE: this class could not be compiled/tested in this environment (no Qt
// or GStreamer toolchain available in the sandbox). It follows the standard
// webrtcbin usage pattern (see gst-plugins-bad's webrtcsendrecv example) but
// should be validated on a real machine before shipping.
class WebRtcPeerConnection : public QObject
{
    Q_OBJECT
public:
    explicit WebRtcPeerConnection(const QString &peerId, QObject *parent = nullptr);
    ~WebRtcPeerConnection() override;

    QString peerId() const { return m_peerId; }

    // Builds and starts the local pipeline. `polite` decides who creates the
    // initial SDP offer when both sides start negotiation simultaneously.
    void start(bool polite);
    void stop();

    void setMicMuted(bool muted);
    void setCameraEnabled(bool enabled);
    void setScreenSharing(bool enabled);

public slots:
    void applyRemoteDescription(const QString &sdpType, const QString &sdp);
    void applyRemoteIceCandidate(const QString &candidate, int mLineIndex);

signals:
    void localDescriptionReady(const QString &peerId, const QString &sdpType, const QString &sdp);
    void localIceCandidateGathered(const QString &peerId, const QString &candidate, int mLineIndex);
    void connectionStateChanged(const QString &peerId, const QString &state);
    void errorOccurred(const QString &peerId, const QString &message);

private:
    static void onNegotiationNeeded(GstElement *webrtcbin, gpointer userData);
    static void onIceCandidate(GstElement *webrtcbin, guint mLineIndex, gchar *candidate, gpointer userData);
    static void onOfferCreated(GstPromise *promise, gpointer userData);
    static void onAnswerCreated(GstPromise *promise, gpointer userData);
    static void onIceConnectionStateNotify(GstElement *webrtcbin, GParamSpec *pspec, gpointer userData);
    static GstBusSyncReply onBusMessage(GstBus *bus, GstMessage *message, gpointer userData);

    void buildPipeline();
    void teardownPipeline();
    void createOffer();
    void createAnswer();

    QString m_peerId;
    bool m_polite = true;
    GstElement *m_pipeline = nullptr;
    GstElement *m_webrtcbin = nullptr;
    GstElement *m_micVolume = nullptr;
    GstElement *m_cameraValve = nullptr;
    GstElement *m_screenValve = nullptr;
    GstBus *m_bus = nullptr;
};

#endif // WEBRTCPEERCONNECTION_H
