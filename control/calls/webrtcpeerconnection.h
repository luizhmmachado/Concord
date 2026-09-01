#ifndef WEBRTCPEERCONNECTION_H
#define WEBRTCPEERCONNECTION_H

#include <QObject>
#if CONCORD_HAS_GSTREAMER
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#endif

class VideoStreamSource;

// Wraps a single `webrtcbin` (one per remote participant). The element itself
// lives in the shared capture pipeline owned by CallMediaEngine; this class
// only drives signaling and renders the inbound streams.
class WebRtcPeerConnection : public QObject
{
    Q_OBJECT
public:
    explicit WebRtcPeerConnection(const QString &peerId, QObject *parent = nullptr);
    ~WebRtcPeerConnection() override;

    QString peerId() const { return m_peerId; }
    VideoStreamSource *remoteVideo() const { return m_remoteVideo; }
    VideoStreamSource *remoteScreen() const { return m_remoteScreen; }

    // `polite` decides who creates the initial SDP offer when both sides start
    // negotiation simultaneously.
#if CONCORD_HAS_GSTREAMER
    void start(bool polite, GstElement *webrtcbin);
#else
    void start(bool polite, void *webrtcbin = nullptr);
#endif
    void stop();

public slots:
    void applyRemoteDescription(const QString &sdpType, const QString &sdp);
    void applyRemoteIceCandidate(const QString &candidate, int mLineIndex);

signals:
    void localDescriptionReady(const QString &peerId, const QString &sdpType, const QString &sdp);
    void localIceCandidateGathered(const QString &peerId, const QString &candidate, int mLineIndex);
    void connectionStateChanged(const QString &peerId, const QString &state);
    void errorOccurred(const QString &peerId, const QString &message);

private:
    QString m_peerId;
    bool m_polite = true;
    VideoStreamSource *m_remoteVideo = nullptr;
    VideoStreamSource *m_remoteScreen = nullptr;

#if CONCORD_HAS_GSTREAMER
    static void onNegotiationNeeded(GstElement *webrtcbin, gpointer userData);
    static void onIceCandidate(GstElement *webrtcbin, guint mLineIndex, gchar *candidate, gpointer userData);
    static void onOfferCreated(GstPromise *promise, gpointer userData);
    static void onAnswerCreated(GstPromise *promise, gpointer userData);
    static void onIceConnectionStateNotify(GstElement *webrtcbin, GParamSpec *pspec, gpointer userData);
    static void onIncomingStream(GstElement *webrtcbin, GstPad *pad, gpointer userData);
    static void onDecodedPad(GstElement *decodebin, GstPad *pad, gpointer userData);

    void createOffer();
    void createAnswer();

    GstElement *m_webrtcbin = nullptr;
#endif
};

#endif // WEBRTCPEERCONNECTION_H
