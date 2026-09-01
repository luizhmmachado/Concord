#ifndef MEDIADEVICECONTROLLER_H
#define MEDIADEVICECONTROLLER_H

#include <QObject>
#include <QStringList>
#include <gst/gst.h>

// Enumerates available audio/video capture and playback devices using
// GStreamer's device monitor, for display on the Settings page.
class MediaDeviceController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList microphones READ microphones NOTIFY devicesChanged)
    Q_PROPERTY(QStringList speakers READ speakers NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameras READ cameras NOTIFY devicesChanged)

public:
    explicit MediaDeviceController(QObject *parent = nullptr);
    ~MediaDeviceController() override;

    QStringList microphones() const { return m_microphones; }
    QStringList speakers() const { return m_speakers; }
    QStringList cameras() const { return m_cameras; }

    Q_INVOKABLE void refresh();

signals:
    void devicesChanged();

private:
    GstDeviceMonitor *m_monitor = nullptr;
    QStringList m_microphones;
    QStringList m_speakers;
    QStringList m_cameras;
};

#endif // MEDIADEVICECONTROLLER_H
