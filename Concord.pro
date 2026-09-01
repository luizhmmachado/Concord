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
    # Fall back to the default install locations if the env var isn't set
    # (e.g. Qt Creator was not restarted after installing GStreamer).
    isEmpty(GSTREAMER_ROOT) {
        exists("C:/Program Files/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/Program Files/gstreamer/1.0/mingw_x86_64
        } else: exists("$$(LOCALAPPDATA)/Programs/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = $$(LOCALAPPDATA)/Programs/gstreamer/1.0/mingw_x86_64
        } else: exists("C:/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/gstreamer/1.0/mingw_x86_64
        }
    }
    isEmpty(GSTREAMER_ROOT): error("GStreamer MinGW x86_64 devel not found. Install it and/or set GSTREAMER_ROOT in Concord.pro to its install path.")
    message("Using GStreamer from: $$GSTREAMER_ROOT")
    INCLUDEPATH += $$GSTREAMER_ROOT/include/gstreamer-1.0 \
                   $$GSTREAMER_ROOT/include/glib-2.0 \
                   $$GSTREAMER_ROOT/lib/glib-2.0/include
    # NOTE: deliberately NOT using "-L$$GSTREAMER_ROOT/lib -lname" here.
    # GStreamer's MinGW package ships its own libmingw32.a/libstdc++ import
    # libs in that lib/ folder; adding it as a -L search path shadows the Qt
    # toolchain's copies and breaks CRT startup linking. Referencing the
    # needed .dll.a files by full path avoids touching the search path.
    LIBS += $$GSTREAMER_ROOT/lib/libgstreamer-1.0.dll.a \
            $$GSTREAMER_ROOT/lib/libgstwebrtc-1.0.dll.a \
            $$GSTREAMER_ROOT/lib/libgstsdp-1.0.dll.a \
            $$GSTREAMER_ROOT/lib/libgstvideo-1.0.dll.a \
            $$GSTREAMER_ROOT/lib/libgobject-2.0.dll.a \
            $$GSTREAMER_ROOT/lib/libglib-2.0.dll.a
}

