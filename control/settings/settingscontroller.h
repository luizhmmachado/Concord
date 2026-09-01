#ifndef SETTINGSCONTROLLER_H
#define SETTINGSCONTROLLER_H

#include <QObject>
#include "control/database/supabaseclient.h"

// Owns the app's theme/accent color and device preferences. Persists
// immediately to QSettings (works offline) and syncs to Supabase
// (user_settings table) in the background so preferences follow the user.
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY notificationsEnabledChanged)
    Q_PROPERTY(QString inputDevice READ inputDevice WRITE setInputDevice NOTIFY inputDeviceChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY outputDeviceChanged)
    Q_PROPERTY(QString videoDevice READ videoDevice WRITE setVideoDevice NOTIFY videoDeviceChanged)

public:
    explicit SettingsController(SupabaseClient *client, QObject *parent = nullptr);

    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);

    QString accentColor() const { return m_accentColor; }
    void setAccentColor(const QString &color);

    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);

    QString inputDevice() const { return m_inputDevice; }
    void setInputDevice(const QString &device);

    QString outputDevice() const { return m_outputDevice; }
    void setOutputDevice(const QString &device);

    QString videoDevice() const { return m_videoDevice; }
    void setVideoDevice(const QString &device);

    Q_INVOKABLE void loadFromLocalStorage();
    Q_INVOKABLE void syncFromRemote();

signals:
    void themeChanged();
    void accentColorChanged();
    void notificationsEnabledChanged();
    void inputDeviceChanged();
    void outputDeviceChanged();
    void videoDeviceChanged();

private:
    void persistLocal();
    void persistRemote();

    SupabaseClient *m_client;
    QString m_theme = "dark";
    QString m_accentColor = "#5865F2";
    bool m_notificationsEnabled = true;
    QString m_inputDevice;
    QString m_outputDevice;
    QString m_videoDevice;
};

#endif // SETTINGSCONTROLLER_H
