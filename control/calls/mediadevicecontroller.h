#ifndef MEDIADEVICECONTROLLER_H
#define MEDIADEVICECONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QAudioDeviceInfo>
#include <QCameraInfo>

// Enumerates available audio/video capture and playback devices using Qt's
// native media APIs so the settings page works without requiring a custom
// GStreamer runtime installation just to list devices.
class MediaDeviceController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList microphones READ microphones NOTIFY devicesChanged)
    Q_PROPERTY(QStringList speakers READ speakers NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameras READ cameras NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameraDeviceIds READ cameraDeviceIds NOTIFY devicesChanged)

public:
    explicit MediaDeviceController(QObject *parent = nullptr);
    ~MediaDeviceController() override;

    QStringList microphones() const { return m_microphones; }
    QStringList speakers() const { return m_speakers; }
    QStringList cameras() const { return m_cameras; }
    QStringList cameraDeviceIds() const { return m_cameraDeviceIds; }

    Q_INVOKABLE void refresh();

signals:
    void devicesChanged();

private:
    QStringList m_microphones;
    QStringList m_speakers;
    QStringList m_cameras;
    QStringList m_cameraDeviceIds;
};

#endif // MEDIADEVICECONTROLLER_H
