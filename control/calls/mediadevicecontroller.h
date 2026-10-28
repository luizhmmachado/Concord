#ifndef MEDIADEVICECONTROLLER_H
#define MEDIADEVICECONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QByteArray>
#include <QAudioDeviceInfo>
#include <QCameraInfo>

class QAudioInput;
class QAudioOutput;
class QTimer;

// Enumerates available audio/video capture and playback devices using Qt's
// native media APIs so the settings page works without requiring a custom
// GStreamer runtime installation just to list devices.
class MediaDeviceController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList microphones READ microphones NOTIFY devicesChanged)
    Q_PROPERTY(QStringList microphoneDeviceIds READ microphoneDeviceIds NOTIFY devicesChanged)
    Q_PROPERTY(QStringList speakers READ speakers NOTIFY devicesChanged)
    Q_PROPERTY(QStringList speakerDeviceIds READ speakerDeviceIds NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameras READ cameras NOTIFY devicesChanged)
    Q_PROPERTY(QStringList cameraDeviceIds READ cameraDeviceIds NOTIFY devicesChanged)
    Q_PROPERTY(bool microphoneMonitoring READ microphoneMonitoring NOTIFY microphoneMonitoringChanged)
    Q_PROPERTY(QString microphoneMonitorError READ microphoneMonitorError NOTIFY microphoneMonitorErrorChanged)

public:
    explicit MediaDeviceController(QObject *parent = nullptr);
    ~MediaDeviceController() override;

    QStringList microphones() const { return m_microphones; }
    QStringList microphoneDeviceIds() const { return m_microphoneDeviceIds; }
    QStringList speakers() const { return m_speakers; }
    QStringList speakerDeviceIds() const { return m_speakerDeviceIds; }
    QStringList cameras() const { return m_cameras; }
    QStringList cameraDeviceIds() const { return m_cameraDeviceIds; }

    bool microphoneMonitoring() const { return m_microphoneMonitoring; }
    QString microphoneMonitorError() const { return m_microphoneMonitorError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startDeviceWatcher();
    Q_INVOKABLE void stopDeviceWatcher();
    Q_INVOKABLE void startMicrophoneMonitoring(const QString &inputDevice,
                                               const QString &outputDevice);
    Q_INVOKABLE void stopMicrophoneMonitoring();

signals:
    void devicesChanged();
    void microphoneMonitoringChanged();
    void microphoneMonitorErrorChanged();

private:
    QAudioDeviceInfo resolveAudioDevice(QAudio::Mode mode,
                                        const QString &deviceId) const;
    void setMicrophoneMonitorError(const QString &message);

    QTimer *m_deviceWatcher = nullptr;
    QAudioInput *m_audioInput = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QByteArray m_monitorPendingData;
    QString m_monitorInputId;
    QString m_monitorOutputId;
    bool m_microphoneMonitoring = false;
    QString m_microphoneMonitorError;
    QStringList m_microphones;
    QStringList m_microphoneDeviceIds;
    QStringList m_speakers;
    QStringList m_speakerDeviceIds;
    QStringList m_cameras;
    QStringList m_cameraDeviceIds;
};

#endif // MEDIADEVICECONTROLLER_H
