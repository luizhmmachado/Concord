pragma Singleton
import QtQuick 2.15

// Central theme singleton. Colors follow `settingsController.theme` /
// `settingsController.accentColor` (set from the Settings page) and persist
// via SettingsController (QSettings + Supabase `user_settings`).
QtObject {
    readonly property string mode: settingsController ? settingsController.theme : "dark"
    readonly property color accent: settingsController ? settingsController.accentColor : "#1FD1C1"
    readonly property color accentSecondary: "#17A398"

    readonly property color background: mode === "light" ? "#FFFFFF" : (mode === "midnight" ? "#000000" : "#1B1D21")
    readonly property color surface: mode === "light" ? "#F2F3F5" : (mode === "midnight" ? "#0A0A0A" : "#232529")
    readonly property color surfaceAlt: mode === "light" ? "#E3E5E8" : (mode === "midnight" ? "#050505" : "#101114")
    readonly property color textPrimary: mode === "light" ? "#060607" : "#F2F3F5"
    readonly property color textSecondary: mode === "light" ? "#5C5E66" : "#8A8D93"
    readonly property color danger: "#DA373C"
    readonly property color online: "#23A55A"

    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 10
    readonly property int spacing: 8
}
