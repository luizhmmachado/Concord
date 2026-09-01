#include "videostreamsource.h"

#include <QAbstractVideoSurface>
#include <QVideoSurfaceFormat>
#include <QVideoFrame>
#include <QMetaObject>
#include <QDebug>

#if CONCORD_HAS_GSTREAMER
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#endif

VideoStreamSource::VideoStreamSource(QObject *parent)
    : QObject(parent)
{
}

VideoStreamSource::~VideoStreamSource()
{
    stopSurface();
}

void VideoStreamSource::setVideoSurface(QAbstractVideoSurface *surface)
{
    qInfo() << "[video]" << objectName() << "setVideoSurface" << surface;
    if (m_surface == surface)
        return;
    stopSurface();
    m_surface = surface;
    m_currentSize = QSize();
}

void VideoStreamSource::stopSurface()
{
    if (m_surface && m_surface->isActive())
        m_surface->stop();
}

void VideoStreamSource::clear()
{
    stopSurface();
    m_currentSize = QSize();
    emit frameSizeChanged();
    if (m_hasFrames) {
        m_hasFrames = false;
        emit hasFramesChanged();
    }
}

void VideoStreamSource::pushFrame(const QImage &image)
{
    QMetaObject::invokeMethod(this, "renderFrame", Qt::QueuedConnection, Q_ARG(QImage, image));
}

void VideoStreamSource::noteSample(int width, int height)
{
    if (m_loggedFirstSample)
        return;
    m_loggedFirstSample = true;
    qInfo() << "[video]" << objectName() << "primeiro sample do GStreamer" << width << "x" << height;
}

void VideoStreamSource::renderFrame(const QImage &image)
{
    if (!m_surface) {
        if (!m_warnedNoSurface) {
            m_warnedNoSurface = true;
            qWarning() << "[video]" << objectName() << "frame recebido sem surface ligada";
        }
        return;
    }
    if (image.isNull())
        return;

    if (m_currentSize != image.size() || !m_surface->isActive()) {
        stopSurface();
        QVideoSurfaceFormat format(image.size(), QVideoFrame::Format_RGB32);
        if (!m_surface->start(format)) {
            qWarning() << "[video]" << objectName() << "surface->start falhou" << image.size()
                       << m_surface->error();
            return;
        }
        qInfo() << "[video]" << objectName() << "surface iniciada" << image.size();
        m_currentSize = image.size();
        emit frameSizeChanged();
    }

    m_surface->present(QVideoFrame(image));

    if (!m_hasFrames) {
        m_hasFrames = true;
        qInfo() << "[video]" << objectName() << "primeiro frame apresentado";
        emit hasFramesChanged();
    }
}

#if CONCORD_HAS_GSTREAMER

namespace {

GstFlowReturn onNewSample(GstElement *sink, gpointer userData)
{
    auto *target = static_cast<VideoStreamSource *>(userData);

    GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample)
        return GST_FLOW_OK;

    GstCaps *caps = gst_sample_get_caps(sample);
    GstVideoInfo info;
    if (!caps || !gst_video_info_from_caps(&info, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        // BGRA in memory order matches Qt's Format_RGB32 on little-endian.
        const QImage frame(map.data, info.width, info.height, stride, QImage::Format_RGB32);
        target->noteSample(info.width, info.height);
        target->pushFrame(frame.copy());
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

} // namespace

void attachAppSinkToVideoSource(GstElement *appsink, VideoStreamSource *target)
{
    if (!appsink || !target)
        return;
    qInfo() << "[video] appsink ligado a" << target->objectName();
    g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE, "max-buffers", 1, "drop", TRUE, nullptr);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(onNewSample), target);
}

#endif // CONCORD_HAS_GSTREAMER
