#include "mediadevicecontroller.h"

#include <QDebug>
#include <QPair>
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
    m_cameraDeviceIds.clear();

    const QList<QAudioDeviceInfo> audioInputs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    for (const QAudioDeviceInfo &device : audioInputs) {
        if (!device.deviceName().isEmpty())
            m_microphones.append(device.deviceName());
    }
    const QString defaultInput = QAudioDeviceInfo::defaultInputDevice().deviceName();
    const int defaultInputIndex = m_microphones.indexOf(defaultInput);
    if (defaultInputIndex > 0)
        m_microphones.move(defaultInputIndex, 0);

    const QList<QAudioDeviceInfo> audioOutputs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (const QAudioDeviceInfo &device : audioOutputs) {
        if (!device.deviceName().isEmpty())
            m_speakers.append(device.deviceName());
    }
    const QString defaultOutput = QAudioDeviceInfo::defaultOutputDevice().deviceName();
    const int defaultOutputIndex = m_speakers.indexOf(defaultOutput);
    if (defaultOutputIndex > 0)
        m_speakers.move(defaultOutputIndex, 0);

    QList<QPair<QString, QString>> camerasList;
    const QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo &camera : cameras) {
        const QString name = camera.description().isEmpty() ? camera.deviceName() : camera.description();
        if (!name.isEmpty())
            camerasList.append(qMakePair(name, camera.deviceName()));
    }
    const QString defaultCamera = QCameraInfo::defaultCamera().deviceName();
    for (int i = 0; i < camerasList.size(); ++i) {
        if (!defaultCamera.isEmpty() && camerasList.at(i).second == defaultCamera && i > 0) {
            camerasList.insert(0, camerasList.takeAt(i));
            break;
        }
    }
    for (const QPair<QString, QString> &camera : camerasList) {
        m_cameras.append(camera.first);
        m_cameraDeviceIds.append(camera.second);
    }

    emit devicesChanged();
}
