QT += core gui qml quick network multimedia websockets quickcontrols2
CONFIG += c++17
TARGET = Concord
TEMPLATE = app

APP_VERSION = $$(APP_VERSION)
isEmpty(APP_VERSION): APP_VERSION = 0.1.0
VERSION = $$APP_VERSION

QMAKE_TARGET_COMPANY = Concord
QMAKE_TARGET_PRODUCT = Concord
QMAKE_TARGET_DESCRIPTION = Concord desktop client
QMAKE_TARGET_COPYRIGHT = Concord

DEFINES += CONCORD_HAS_GSTREAMER=0
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"

SOURCES += \
    main.cpp \
    control/database/supabaseclient.cpp \
    control/clipboard/clipboardcontroller.cpp \
    control/images/imagecontroller.cpp \
    control/database/realtimeclient.cpp \
    control/auth/authcontroller.cpp \
    control/servers/servercontroller.cpp \
    control/servers/serverlistmodel.cpp \
    control/servers/channellistmodel.cpp \
    control/servers/servermembermodel.cpp \
    control/servers/serverrolemodel.cpp \
    control/servers/servermemberscontroller.cpp \
    control/presence/onlinepresencecontroller.cpp \
    control/chat/chatcontroller.cpp \
    control/chat/messagelistmodel.cpp \
    control/dm/directmessagecontroller.cpp \
    control/dm/conversationlistmodel.cpp \
    control/settings/settingscontroller.cpp \
    control/calls/callcontroller.cpp \
    control/calls/webrtcpeerconnection.cpp \
    control/calls/callparticipantmodel.cpp \
    control/calls/callpresencecontroller.cpp \
    control/calls/callmediaengine.cpp \
    control/calls/videostreamsource.cpp \
    control/calls/screensharesourcemodel.cpp \
    control/calls/mediadevicecontroller.cpp

HEADERS += \
    control/database/supabaseclient.h \
    control/clipboard/clipboardcontroller.h \
    control/images/imagecontroller.h \
    control/database/realtimeclient.h \
    control/auth/authcontroller.h \
    control/servers/servercontroller.h \
    control/servers/serverlistmodel.h \
    control/servers/channellistmodel.h \
    control/servers/servermembermodel.h \
    control/servers/serverrolemodel.h \
    control/servers/servermemberscontroller.h \
    control/presence/onlinepresencecontroller.h \
    control/chat/chatcontroller.h \
    control/chat/messagelistmodel.h \
    control/dm/directmessagecontroller.h \
    control/dm/conversationlistmodel.h \
    control/settings/settingscontroller.h \
    control/calls/callcontroller.h \
    control/calls/webrtcpeerconnection.h \
    control/calls/callparticipantmodel.h \
    control/calls/callpresencecontroller.h \
    control/calls/callmediaengine.h \
    control/calls/videostreamsource.h \
    control/calls/screensharesourcemodel.h \
    control/calls/mediadevicecontroller.h

RESOURCES += \
    qml.qrc

INCLUDEPATH += $$PWD

linux {
    system(pkg-config --exists xcb) {
        CONFIG += link_pkgconfig
        PKGCONFIG += xcb
    } else {
        error("libxcb development files are required on Linux (install libxcb1-dev).")
    }
}

unix {
    GSTREAMER_PKG_DEPS = gstreamer-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 gstreamer-video-1.0 gstreamer-app-1.0
    system(pkg-config --exists $$GSTREAMER_PKG_DEPS) {
        CONFIG += link_pkgconfig
        PKGCONFIG += $$GSTREAMER_PKG_DEPS
        DEFINES -= CONCORD_HAS_GSTREAMER=0
        DEFINES += CONCORD_HAS_GSTREAMER=1
        message("GStreamer development package found. Calls enabled.")
    } else {
        warning("GStreamer development package not found. Building without call media pipelines.")
    }
}

win32 {
    # Set GSTREAMER_ROOT (or the installer's environment variable) to the
    # GStreamer x86_64 SDK directory. Both MSVC and MinGW toolchains are
    # supported; CI uses the official MSVC package.
    isEmpty(GSTREAMER_ROOT): GSTREAMER_ROOT = $$(GSTREAMER_ROOT)
    isEmpty(GSTREAMER_ROOT): GSTREAMER_ROOT = $$(GSTREAMER_1_0_ROOT_MSVC_X86_64)
    isEmpty(GSTREAMER_ROOT): GSTREAMER_ROOT = $$(GSTREAMER_1_0_ROOT_MINGW_X86_64)

    isEmpty(GSTREAMER_ROOT) {
        exists("C:/Program Files/gstreamer/1.0/msvc_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/Program Files/gstreamer/1.0/msvc_x86_64
        } else: exists("$$(LOCALAPPDATA)/Programs/gstreamer/1.0/msvc_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = $$(LOCALAPPDATA)/Programs/gstreamer/1.0/msvc_x86_64
        } else: exists("C:/gstreamer/1.0/msvc_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/gstreamer/1.0/msvc_x86_64
        } else: exists("C:/Program Files/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/Program Files/gstreamer/1.0/mingw_x86_64
        } else: exists("$$(LOCALAPPDATA)/Programs/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = $$(LOCALAPPDATA)/Programs/gstreamer/1.0/mingw_x86_64
        } else: exists("C:/gstreamer/1.0/mingw_x86_64/include/gstreamer-1.0/gst/gst.h") {
            GSTREAMER_ROOT = C:/gstreamer/1.0/mingw_x86_64
        }
    }

    !isEmpty(GSTREAMER_ROOT): GSTREAMER_ROOT = $$clean_path($$GSTREAMER_ROOT)

    msvc {
        QMAKE_CXXFLAGS += /utf-8
        GST_LIB_PREFIX =
        GST_LIB_EXT = .lib
    } else {
        GST_LIB_PREFIX = lib
        GST_LIB_EXT = .dll.a
    }

    !isEmpty(GSTREAMER_ROOT):exists("$$GSTREAMER_ROOT/include/gstreamer-1.0/gst/gst.h") {
        message("Using GStreamer from: $$GSTREAMER_ROOT")
        INCLUDEPATH += "$$GSTREAMER_ROOT/include/gstreamer-1.0" \
                       "$$GSTREAMER_ROOT/include/glib-2.0" \
                       "$$GSTREAMER_ROOT/lib/glib-2.0/include"

        # Use full import-library paths instead of -L search paths: the MinGW
        # GStreamer package ships its own CRT/C++ runtime libs that must not
        # shadow the Qt toolchain's copies.
        GST_LIBS = gstreamer-1.0 gstwebrtc-1.0 gstsdp-1.0 gstvideo-1.0 \
                   gstapp-1.0 gobject-2.0 glib-2.0
        for(gstlib, GST_LIBS) {
            LIBS += "$$GSTREAMER_ROOT/lib/$${GST_LIB_PREFIX}$${gstlib}$${GST_LIB_EXT}"
        }

        DEFINES -= CONCORD_HAS_GSTREAMER=0
        DEFINES += CONCORD_HAS_GSTREAMER=1
    } else {
        warning("GStreamer x86_64 development package not found. Building without call media pipelines.")
    }
}

