#include "settingscontroller.h"

#include <QSettings>

SettingsController::SettingsController(SupabaseClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &tag, bool ok, const QJsonArray &rows, const QString &) {
        if (tag != "settings:load" || !ok || rows.isEmpty())
            return;
        const QJsonObject row = rows.first().toObject();
        m_theme = row.value("theme").toString(m_theme);
        m_accentColor = row.value("accent_color").toString(m_accentColor);
        m_notificationsEnabled = row.value("notifications_enabled").toBool(m_notificationsEnabled);
        m_inputDevice = row.value("input_device").toString();
        m_outputDevice = row.value("output_device").toString();
        m_videoDevice = row.value("video_device").toString();
        persistLocal();
        emit themeChanged();
        emit accentColorChanged();
        emit notificationsEnabledChanged();
        emit inputDeviceChanged();
        emit outputDeviceChanged();
        emit videoDeviceChanged();
    });

    loadFromLocalStorage();
}

void SettingsController::loadFromLocalStorage()
{
    QSettings settings;
    settings.beginGroup("settings");
    m_theme = settings.value("theme", m_theme).toString();
    m_accentColor = settings.value("accentColor", m_accentColor).toString();
    m_notificationsEnabled = settings.value("notificationsEnabled", m_notificationsEnabled).toBool();
    m_inputDevice = settings.value("inputDevice").toString();
    m_outputDevice = settings.value("outputDevice").toString();
    m_videoDevice = settings.value("videoDevice").toString();
    settings.endGroup();
}

void SettingsController::syncFromRemote()
{
    if (!m_client->isAuthenticated())
        return;
    m_client->select(QStringLiteral("user_settings?user_id=eq.%1&select=*").arg(m_client->currentUserId()), "settings:load");
}

void SettingsController::persistLocal()
{
    QSettings settings;
    settings.beginGroup("settings");
    settings.setValue("theme", m_theme);
    settings.setValue("accentColor", m_accentColor);
    settings.setValue("notificationsEnabled", m_notificationsEnabled);
    settings.setValue("inputDevice", m_inputDevice);
    settings.setValue("outputDevice", m_outputDevice);
    settings.setValue("videoDevice", m_videoDevice);
    settings.endGroup();
}

void SettingsController::persistRemote()
{
    if (!m_client->isAuthenticated())
        return;
    m_client->update(QStringLiteral("user_settings?user_id=eq.%1").arg(m_client->currentUserId()),
                      QJsonObject{
                          {"theme", m_theme},
                          {"accent_color", m_accentColor},
                          {"notifications_enabled", m_notificationsEnabled},
                          {"input_device", m_inputDevice},
                          {"output_device", m_outputDevice},
                          {"video_device", m_videoDevice},
                      },
                      "settings:save");
}

void SettingsController::setTheme(const QString &theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    persistLocal();
    persistRemote();
    emit themeChanged();
}

void SettingsController::setAccentColor(const QString &color)
{
    if (m_accentColor == color)
        return;
    m_accentColor = color;
    persistLocal();
    persistRemote();
    emit accentColorChanged();
}

void SettingsController::setNotificationsEnabled(bool enabled)
{
    if (m_notificationsEnabled == enabled)
        return;
    m_notificationsEnabled = enabled;
    persistLocal();
    persistRemote();
    emit notificationsEnabledChanged();
}

void SettingsController::setInputDevice(const QString &device)
{
    if (m_inputDevice == device)
        return;
    m_inputDevice = device;
    persistLocal();
    persistRemote();
    emit inputDeviceChanged();
}

void SettingsController::setOutputDevice(const QString &device)
{
    if (m_outputDevice == device)
        return;
    m_outputDevice = device;
    persistLocal();
    persistRemote();
    emit outputDeviceChanged();
}

void SettingsController::setVideoDevice(const QString &device)
{
    if (m_videoDevice == device)
        return;
    m_videoDevice = device;
    persistLocal();
    persistRemote();
    emit videoDeviceChanged();
}
