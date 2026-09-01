QT += core gui qml quick network multimedia websockets quickcontrols2
CONFIG += c++17
TARGET = Concord
TEMPLATE = app

SOURCES += \
    main.cpp \
    control/database/supabaseclient.cpp \
    control/database/realtimeclient.cpp \
    control/auth/authcontroller.cpp \
    control/servers/servercontroller.cpp \
    control/servers/serverlistmodel.cpp \
    control/servers/channellistmodel.cpp \
    control/chat/chatcontroller.cpp \
    control/chat/messagelistmodel.cpp \
    control/settings/settingscontroller.cpp \
    control/calls/callcontroller.cpp \
    control/calls/webrtcpeerconnection.cpp \
    control/calls/callparticipantmodel.cpp \
    control/calls/mediadevicecontroller.cpp

HEADERS += \
    control/database/supabaseclient.h \
    control/database/realtimeclient.h \
    control/auth/authcontroller.h \
    control/servers/servercontroller.h \
    control/servers/serverlistmodel.h \
    control/servers/channellistmodel.h \
    control/chat/chatcontroller.h \
    control/chat/messagelistmodel.h \
    control/settings/settingscontroller.h \
    control/calls/callcontroller.h \
    control/calls/webrtcpeerconnection.h \
    control/calls/callparticipantmodel.h \
    control/calls/mediadevicecontroller.h

RESOURCES += \
    qml.qrc

INCLUDEPATH += $$PWD

# --- GStreamer (WebRTC calls: camera/mic/screen capture + webrtcbin) ---
# Requires GStreamer 1.18+ with gstreamer-plugins-{base,good,bad,ugly} and
# gst-plugins-bad's webrtc/nice/sdp modules installed on the build machine.
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += gstreamer-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 gstreamer-video-1.0
}
win32 {
    # Adjust GSTREAMER_ROOT to your local GStreamer MinGW SDK installation,
    # e.g. C:/gstreamer/1.0/mingw_x86_64
    isEmpty(GSTREAMER_ROOT): GSTREAMER_ROOT = $$(GSTREAMER_1_0_ROOT_MINGW_X86_64)
    INCLUDEPATH += $$GSTREAMER_ROOT/include/gstreamer-1.0 \
                   $$GSTREAMER_ROOT/include/glib-2.0 \
                   $$GSTREAMER_ROOT/lib/glib-2.0/include
    LIBS += -L$$GSTREAMER_ROOT/lib \
            -lgstreamer-1.0 -lgstwebrtc-1.0 -lgstsdp-1.0 -lgstvideo-1.0 \
            -lgobject-2.0 -lglib-2.0
}
