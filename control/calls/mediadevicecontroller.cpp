#include "mediadevicecontroller.h"

MediaDeviceController::MediaDeviceController(QObject *parent)
    : QObject(parent)
{
    m_monitor = gst_device_monitor_new();
    gst_device_monitor_add_filter(m_monitor, "Audio/Source", nullptr);
    gst_device_monitor_add_filter(m_monitor, "Audio/Sink", nullptr);
    gst_device_monitor_add_filter(m_monitor, "Video/Source", nullptr);
}

MediaDeviceController::~MediaDeviceController()
{
    if (m_monitor) {
        gst_device_monitor_stop(m_monitor);
        gst_object_unref(m_monitor);
        m_monitor = nullptr;
    }
}

void MediaDeviceController::refresh()
{
    m_microphones.clear();
    m_speakers.clear();
    m_cameras.clear();

    GList *devices = gst_device_monitor_get_devices(m_monitor);
    for (GList *item = devices; item != nullptr; item = item->next) {
        auto *device = static_cast<GstDevice *>(item->data);
        gchar *name = gst_device_get_display_name(device);
        gchar *deviceClass = gst_device_get_device_class(device);
        const QString qName = QString::fromUtf8(name);
        const QString qClass = QString::fromUtf8(deviceClass);
        g_free(name);
        g_free(deviceClass);

        if (qClass.startsWith("Audio/Source"))
            m_microphones.append(qName);
        else if (qClass.startsWith("Audio/Sink"))
            m_speakers.append(qName);
        else if (qClass.startsWith("Video/Source"))
            m_cameras.append(qName);

        gst_object_unref(device);
    }
    g_list_free(devices);

    emit devicesChanged();
}
