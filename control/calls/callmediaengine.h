#ifndef CALLMEDIAENGINE_H
#define CALLMEDIAENGINE_H

#include <QObject>
#include <QString>
#include <map>
#include <vector>

#if CONCORD_HAS_GSTREAMER
#include <gst/gst.h>
#endif

class VideoStreamSource;

// Owns the single capture pipeline shared by every peer in a call: one camera,
// one screen capture and one microphone feed a `tee` each, and every peer gets
// its own encoder branch plus `webrtcbin` grafted onto those tees.
//
// Capture devices are only opened while the corresponding feature is enabled;
// a black `videotestsrc` keeps the tees flowing in the meantime so peers never
// see an EOS when the user turns the camera off.
class CallMediaEngine : public QObject
{
    Q_OBJECT
public:
    explicit CallMediaEngine(QObject *parent = nullptr);
    ~CallMediaEngine() override;

    bool start();
    void stop();
    bool isRunning() const;

    VideoStreamSource *cameraPreview() const { return m_cameraPreview; }
    VideoStreamSource *screenPreview() const { return m_screenPreview; }

    void setCameraEnabled(bool enabled);
    void setScreenSharingEnabled(bool enabled);
    // Overrides the source used by the next screen-share start. An empty
    // description falls back to capturing the whole primary screen.
    void setScreenCaptureDescription(const QString &description);
    void setScreenShareSettings(int height, int fps);
    int screenShareHeight() const { return m_screenShareHeight; }
    int screenShareFps() const { return m_screenShareFps; }
    void setScreenPreviewEnabled(bool enabled);
    void setMicMuted(bool muted);

#if CONCORD_HAS_GSTREAMER
    // Returns the peer's `webrtcbin` (owned by the pipeline) or nullptr. The
    // element stays paused so callers can connect their signal handlers before
    // startPeerBin() triggers negotiation.
    GstElement *createPeerBin(const QString &peerId);
#else
    void *createPeerBin(const QString &peerId);
#endif
    void startPeerBin(const QString &peerId);
    void removePeerBin(const QString &peerId);

signals:
    void errorOccurred(const QString &message);

private:
#if CONCORD_HAS_GSTREAMER
    struct PeerBranches {
        GstElement *webrtcbin = nullptr;
        std::vector<GstElement *> bins;
        std::vector<std::pair<GstElement *, GstPad *>> teePads;
    };

    GstElement *addLiveSource(const QString &description, GstElement *selector, GstElement **selectorPadOwner);
    void removeLiveSource(GstElement **source, GstElement *selector, GstPad **pad);
    void selectPad(GstElement *selector, GstPad *pad);
    GstElement *attachBranch(GstElement *tee, const QString &description, PeerBranches &peer);
    void applyScreenShareEncoding();

    GstElement *m_pipeline = nullptr;
    GstElement *m_cameraSelector = nullptr;
    GstElement *m_screenSelector = nullptr;
    GstElement *m_micVolume = nullptr;
    GstElement *m_cameraTee = nullptr;
    GstElement *m_screenTee = nullptr;
    GstElement *m_micTee = nullptr;
    GstElement *m_screenPreviewValve = nullptr;
    GstElement *m_cameraSource = nullptr;
    GstElement *m_screenSource = nullptr;
    GstPad *m_cameraBlackPad = nullptr;
    GstPad *m_screenBlackPad = nullptr;
    GstPad *m_cameraLivePad = nullptr;
    GstPad *m_screenLivePad = nullptr;
    GstBus *m_bus = nullptr;

    std::map<QString, PeerBranches> m_peers;
#endif

    VideoStreamSource *m_cameraPreview = nullptr;
    VideoStreamSource *m_screenPreview = nullptr;
    QString m_screenCaptureDescription;
    int m_screenShareHeight = 1080;
    int m_screenShareFps = 30;
    bool m_screenPreviewEnabled = false;
    bool m_cameraEnabled = false;
    bool m_screenSharing = false;
    bool m_micMuted = false;
};

#endif // CALLMEDIAENGINE_H
