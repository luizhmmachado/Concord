#ifndef VIDEOSTREAMSOURCE_H
#define VIDEOSTREAMSOURCE_H

#include <QObject>
#include <QImage>
#include <QSize>

#if CONCORD_HAS_GSTREAMER
#include <gst/gst.h>
#endif

class QAbstractVideoSurface;

// Bridges decoded video frames into a QML `VideoOutput` by exposing the
// `videoSurface` property that VideoOutput binds to when used as a source.
class VideoStreamSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractVideoSurface *videoSurface READ videoSurface WRITE setVideoSurface)
    Q_PROPERTY(bool hasFrames READ hasFrames NOTIFY hasFramesChanged)
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameSizeChanged)

public:
    explicit VideoStreamSource(QObject *parent = nullptr);
    ~VideoStreamSource() override;

    QAbstractVideoSurface *videoSurface() const { return m_surface; }
    void setVideoSurface(QAbstractVideoSurface *surface);

    bool hasFrames() const { return m_hasFrames; }
    QSize frameSize() const { return m_currentSize; }

    // Safe to call from a GStreamer streaming thread; the frame is delivered
    // to the surface on the GUI thread.
    void pushFrame(const QImage &image);

    // Diagnostics: logs the first sample seen on the streaming thread.
    void noteSample(int width, int height);

    void clear();

signals:
    void hasFramesChanged();
    void frameSizeChanged();

private slots:
    void renderFrame(const QImage &image);

private:
    void stopSurface();

    QAbstractVideoSurface *m_surface = nullptr;
    QSize m_currentSize;
    bool m_hasFrames = false;
    bool m_warnedNoSurface = false;
    bool m_loggedFirstSample = false;
};

#if CONCORD_HAS_GSTREAMER
// Wires an `appsink` (caps: video/x-raw,format=BGRA) so every produced sample
// is converted to a QImage and pushed into `target`.
void attachAppSinkToVideoSource(GstElement *appsink, VideoStreamSource *target);
#endif

#endif // VIDEOSTREAMSOURCE_H
