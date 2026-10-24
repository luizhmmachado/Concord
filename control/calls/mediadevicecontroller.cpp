#include "mediadevicecontroller.h"

#include <QDebug>
MediaDeviceController::MediaDeviceController(QObject *parent)
    : QObject(parent)
{
    qInfo().noquote() << "MediaDeviceController::MediaDeviceController";
    refresh();
}

MediaDeviceController::~MediaDeviceController() {
    qInfo().noquote() << "MediaDeviceController::~MediaDeviceController";
}

void MediaDeviceController::refresh()
{
    qInfo().noquote() << "MediaDeviceController::refresh";
    m_microphones.clear();
    m_speakers.clear();
    m_cameras.clear();

    const QList<QAudioDeviceInfo> audioInputs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo &device : audioInputs) {
        if (!device.deviceName().isEmpty())
            m_microphones.append(device.deviceName());
    }

    const QList<QAudioDeviceInfo> audioOutputs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (const QAudioDeviceInfo &device : audioOutputs) {
        if (!device.deviceName().isEmpty())
            m_speakers.append(device.deviceName());
    }

    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &camera : cameras) {
        const QString name = camera.description().isEmpty() ? camera.deviceName() : camera.description();
        if (!name.isEmpty())
            m_cameras.append(name);
    }

    emit devicesChanged();
}
