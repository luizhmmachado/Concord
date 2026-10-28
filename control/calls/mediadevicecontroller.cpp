#include "mediadevicecontroller.h"

#include <QAudio>
#include <QAudioFormat>
#include <QAudioInput>
#include <QAudioOutput>
#include <QDebug>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPair>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

namespace {

#ifdef Q_OS_LINUX
void moveDeviceToFront(const QString &id, QStringList *names, QStringList *ids)
{
    const int index = ids->indexOf(id);
    if (index > 0) {
        ids->move(index, 0);
        names->move(index, 0);
    }
}

void appendPipeWireDevice(QStringList *names, QStringList *ids,
                          const QString &description, const QString &nodeName)
{
    if (nodeName.isEmpty())
        return;
    if (names->contains(description) && ids->contains(nodeName))
        return;
    names->append(description.isEmpty() ? nodeName : description);
    ids->append(nodeName);
}

bool loadPipeWireAudioDevices(QStringList *inputNames, QStringList *inputIds,
                              QStringList *outputNames, QStringList *outputIds)
{
    QProcess process;
    process.start("pw-dump", QStringList());
    if (!process.waitForStarted(1000))
        return false;
    if (!process.waitForFinished(5000))
        return false;
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return false;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return false;

    QStringList sources;
    QStringList sourceIds;
    QStringList sinks;
    QStringList sinkIds;
    QString defaultSource;
    QString defaultSink;

    for (const QJsonValue &value : document.array()) {
        if (!value.isObject())
            continue;

        const QJsonObject object = value.toObject();
        const QString type = object.value("type").toString();

        if (type == "PipeWire:Interface:Node") {
            const QJsonObject props = object.value("info").toObject()
                                          .value("props").toObject();
            const QString mediaClass = props.value("media.class").toString();
            const QString nodeName = props.value("node.name").toString();
            QString description = props.value("node.description").toString();
            if (description.isEmpty())
                description = props.value("node.nick").toString();

            if (nodeName.contains(".monitor", Qt::CaseInsensitive)
                    || description.startsWith("Monitor of", Qt::CaseInsensitive))
                continue;

            if (mediaClass == "Audio/Source")
                appendPipeWireDevice(&sources, &sourceIds, description, nodeName);
            else if (mediaClass == "Audio/Sink")
                appendPipeWireDevice(&sinks, &sinkIds, description, nodeName);
        } else if (type == "PipeWire:Interface:Metadata") {
            const QJsonArray metadata = object.value("metadata").toArray();
            for (const QJsonValue &entryValue : metadata) {
                const QJsonObject entry = entryValue.toObject();
                const QString key = entry.value("key").toString();
                if (key != "default.audio.source" && key != "default.audio.sink")
                    continue;
                const QString name = entry.value("value").toObject()
                                         .value("name").toString();
                if (key == "default.audio.source")
                    defaultSource = name;
                else
                    defaultSink = name;
            }
        }
    }

    if (sources.isEmpty() && sinks.isEmpty())
        return false;

    moveDeviceToFront(defaultSource, &sources, &sourceIds);
    moveDeviceToFront(defaultSink, &sinks, &sinkIds);

    *inputNames = sources;
    *inputIds = sourceIds;
    *outputNames = sinks;
    *outputIds = sinkIds;
    return true;
}
#endif

bool looksTechnical(const QString &name)
{
    return name.startsWith('.')
            || name.contains("alsa_", Qt::CaseInsensitive)
            || name.contains("bluez_", Qt::CaseInsensitive)
            || name.contains("CARD=", Qt::CaseInsensitive)
            || name.startsWith("plughw:", Qt::CaseInsensitive)
            || name.startsWith("hw:", Qt::CaseInsensitive)
            || name.startsWith("usb-", Qt::CaseInsensitive)
            || name.startsWith("pci-", Qt::CaseInsensitive)
            || name.startsWith("platform-", Qt::CaseInsensitive);
}

QString titleCaseToken(const QString &token)
{
    static const QSet<QString> acronyms = {
        "usb", "hdmi", "aux", "mic", "spdif", "iec958", "pci", "hda",
        "ac97", "bluetooth", "bt", "dsp", "pc"
    };
    const QString lower = token.toLower();
    if (acronyms.contains(lower))
        return lower.toUpper();
    if (token != lower)
        return token;
    QString result = token;
    result[0] = result.at(0).toUpper();
    return result;
}

bool isHardwareIdToken(const QString &token)
{
    static const QRegularExpression hexId(
        "^[0-9a-f]{1,16}$", QRegularExpression::CaseInsensitiveOption);
    if (!hexId.match(token).hasMatch())
        return false;
    for (const QChar c : token) {
        if (c.isDigit())
            return true;
    }
    return false;
}

QString friendlyAudioDeviceName(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || !looksTechnical(trimmed))
        return trimmed;

    QString name = trimmed;
    if (name.startsWith('.'))
        name.remove(0, 1);

    name.remove(QRegularExpression(
        "^(alsa|bluez|oss|pulse|pipewire|jack)_(input|output)\\.",
        QRegularExpression::CaseInsensitiveOption));
    name.remove(QRegularExpression(
        "^plug(hw)?:", QRegularExpression::CaseInsensitiveOption));
    name.remove(QRegularExpression(
        "^hw:", QRegularExpression::CaseInsensitiveOption));
    name.replace("CARD=", QString(), Qt::CaseInsensitive);
    name.replace("DEV=", QString(), Qt::CaseInsensitive);
    name.remove(QRegularExpression(
        "\\.(mono|stereo)-fallback$", QRegularExpression::CaseInsensitiveOption));

    name.replace(QRegularExpression("[_.:,]+"), " ");
    name.replace('-', ' ');

    const QStringList tokens = name.split(
        QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList words;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);
        if (i == 0 && tokens.size() > 1) {
            const QString lower = token.toLower();
            if (lower == "usb" || lower == "pci" || lower == "platform"
                    || lower == "pnp")
                continue;
        }
        if (isHardwareIdToken(token))
            continue;
        words.append(titleCaseToken(token));
    }

    QStringList unique;
    for (const QString &word : words) {
        if (unique.isEmpty()
                || unique.last().compare(word, Qt::CaseInsensitive) != 0)
            unique.append(word);
    }

    const QString friendly = unique.join(' ').trimmed();
    return friendly.isEmpty() ? trimmed : friendly;
}

} // namespace

MediaDeviceController::MediaDeviceController(QObject *parent)
    : QObject(parent)
{
    qInfo().noquote() << "MediaDeviceController::MediaDeviceController";
    refresh();
}

MediaDeviceController::~MediaDeviceController() {
    qInfo().noquote() << "MediaDeviceController::~MediaDeviceController";
    stopMicrophoneMonitoring();
}

void MediaDeviceController::refresh()
{
    QStringList microphones;
    QStringList microphoneIds;
    QStringList speakers;
    QStringList speakerIds;
    QStringList cameras;
    QStringList cameraIds;

    bool audioDevicesLoaded = false;
#ifdef Q_OS_LINUX
    audioDevicesLoaded = loadPipeWireAudioDevices(
        &microphones, &microphoneIds, &speakers, &speakerIds);
#endif

    if (!audioDevicesLoaded) {
        const QList<QAudioDeviceInfo> audioInputs = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
        for (const QAudioDeviceInfo &device : audioInputs) {
            const QString deviceId = device.deviceName();
            if (deviceId.isEmpty())
                continue;
            microphoneIds.append(deviceId);
            microphones.append(friendlyAudioDeviceName(deviceId));
        }
        const QString defaultInput = QAudioDeviceInfo::defaultInputDevice().deviceName();
        const int defaultInputIndex = microphoneIds.indexOf(defaultInput);
        if (defaultInputIndex > 0) {
            microphoneIds.move(defaultInputIndex, 0);
            microphones.move(defaultInputIndex, 0);
        }

        const QList<QAudioDeviceInfo> audioOutputs = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
        for (const QAudioDeviceInfo &device : audioOutputs) {
            const QString deviceId = device.deviceName();
            if (deviceId.isEmpty())
                continue;
            speakerIds.append(deviceId);
            speakers.append(friendlyAudioDeviceName(deviceId));
        }
        const QString defaultOutput = QAudioDeviceInfo::defaultOutputDevice().deviceName();
        const int defaultOutputIndex = speakerIds.indexOf(defaultOutput);
        if (defaultOutputIndex > 0) {
            speakerIds.move(defaultOutputIndex, 0);
            speakers.move(defaultOutputIndex, 0);
        }
    }

    QList<QPair<QString, QString>> camerasList;
    const QList<QCameraInfo> cameraInfos = QCameraInfo::availableCameras();
    for (const QCameraInfo &camera : cameraInfos) {
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
        cameras.append(camera.first);
        cameraIds.append(camera.second);
    }

    if (microphones == m_microphones
            && microphoneIds == m_microphoneDeviceIds
            && speakers == m_speakers
            && speakerIds == m_speakerDeviceIds
            && cameras == m_cameras
            && cameraIds == m_cameraDeviceIds)
        return;

    m_microphones = microphones;
    m_microphoneDeviceIds = microphoneIds;
    m_speakers = speakers;
    m_speakerDeviceIds = speakerIds;
    m_cameras = cameras;
    m_cameraDeviceIds = cameraIds;

    if (m_microphoneMonitoring
            && (!m_microphoneDeviceIds.contains(m_monitorInputId)
                || !m_speakerDeviceIds.contains(m_monitorOutputId)))
        stopMicrophoneMonitoring();

    emit devicesChanged();
}

void MediaDeviceController::startDeviceWatcher()
{
    if (!m_deviceWatcher) {
        m_deviceWatcher = new QTimer(this);
        m_deviceWatcher->setInterval(2000);
        connect(m_deviceWatcher, &QTimer::timeout, this, &MediaDeviceController::refresh);
    }
    if (!m_deviceWatcher->isActive())
        m_deviceWatcher->start();
}

void MediaDeviceController::stopDeviceWatcher()
{
    if (m_deviceWatcher)
        m_deviceWatcher->stop();
}

QAudioDeviceInfo MediaDeviceController::resolveAudioDevice(QAudio::Mode mode,
                                                           const QString &deviceId) const
{
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(mode);

    for (const QAudioDeviceInfo &device : devices) {
        if (device.deviceName() == deviceId)
            return device;
    }

    const QStringList &ids = mode == QAudio::AudioInput
                                 ? m_microphoneDeviceIds : m_speakerDeviceIds;
    const QStringList &names = mode == QAudio::AudioInput
                                   ? m_microphones : m_speakers;
    const int index = ids.indexOf(deviceId);
    if (index >= 0 && index < names.size()) {
        const QString &description = names.at(index);
        for (const QAudioDeviceInfo &device : devices) {
            if (device.deviceName().compare(description, Qt::CaseInsensitive) == 0)
                return device;
        }
    }

    return mode == QAudio::AudioInput
               ? QAudioDeviceInfo::defaultInputDevice()
               : QAudioDeviceInfo::defaultOutputDevice();
}

void MediaDeviceController::setMicrophoneMonitorError(const QString &message)
{
    if (m_microphoneMonitorError == message)
        return;
    m_microphoneMonitorError = message;
    emit microphoneMonitorErrorChanged();
}

void MediaDeviceController::startMicrophoneMonitoring(const QString &inputDevice,
                                                       const QString &outputDevice)
{
    stopMicrophoneMonitoring();

    const QAudioDeviceInfo input = resolveAudioDevice(QAudio::AudioInput, inputDevice);
    const QAudioDeviceInfo output = resolveAudioDevice(QAudio::AudioOutput, outputDevice);
    if (input.isNull() || output.isNull()) {
        setMicrophoneMonitorError("Nenhum dispositivo de áudio disponível para o teste.");
        return;
    }

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::SignedInt);
    if (!input.isFormatSupported(format))
        format = input.nearestFormat(format);

    QAudioFormat outputFormat = format;
    if (!output.isFormatSupported(outputFormat))
        outputFormat = output.nearestFormat(outputFormat);

    m_audioInput = new QAudioInput(input, format, this);
    m_audioOutput = new QAudioOutput(output, outputFormat, this);
    m_audioInput->setBufferSize(8192);
    m_audioOutput->setBufferSize(8192);
    m_monitorPendingData.clear();

    QIODevice *inputDeviceStream = m_audioInput->start();
    QIODevice *outputDeviceStream = m_audioOutput->start();
    if (!inputDeviceStream || !outputDeviceStream) {
        stopMicrophoneMonitoring();
        setMicrophoneMonitorError("Não foi possível iniciar o teste do microfone.");
        return;
    }

    connect(inputDeviceStream, &QIODevice::readyRead, this,
            [this, inputDeviceStream, outputDeviceStream]() {
        m_monitorPendingData.append(inputDeviceStream->readAll());
        if (m_monitorPendingData.isEmpty())
            return;
        const qint64 written = outputDeviceStream->write(m_monitorPendingData);
        if (written > 0)
            m_monitorPendingData.remove(0, static_cast<int>(written));
        if (m_monitorPendingData.size() > 65536)
            m_monitorPendingData.clear();
    });

    m_monitorInputId = inputDevice;
    m_monitorOutputId = outputDevice;
    setMicrophoneMonitorError(QString());
    m_microphoneMonitoring = true;
    emit microphoneMonitoringChanged();
}

void MediaDeviceController::stopMicrophoneMonitoring()
{
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;
    }
    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
    m_monitorPendingData.clear();
    m_monitorInputId.clear();
    m_monitorOutputId.clear();

    if (m_microphoneMonitoring) {
        m_microphoneMonitoring = false;
        emit microphoneMonitoringChanged();
    }
    setMicrophoneMonitorError(QString());
}
