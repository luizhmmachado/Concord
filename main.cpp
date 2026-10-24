#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QQuickImageProvider>
#include <QPixmap>
#include <QScreen>
#include <QFile>
#if CONCORD_HAS_GSTREAMER
#include <gst/gst.h>
#endif

#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "control/auth/authcontroller.h"
#include "control/servers/servercontroller.h"
#include "control/servers/servermemberscontroller.h"
#include "control/presence/onlinepresencecontroller.h"
#include "control/chat/chatcontroller.h"
#include "control/dm/directmessagecontroller.h"
#include "control/settings/settingscontroller.h"
#include "control/calls/callcontroller.h"
#include "control/calls/callpresencecontroller.h"
#include "control/calls/mediadevicecontroller.h"

// TODO: move these to a local, git-ignored config file before shipping;
// they are safe to ship only because the anon key is meant to be public
// and all data access is protected by Postgres Row Level Security (see
// database policies configured on the Supabase project).
static const QString kSupabaseUrl = QStringLiteral("https://vwhkcthqvofyadjforcf.supabase.co");
static const QString kSupabaseAnonKey = QStringLiteral("sb_publishable_Qg9GqLbfC4kttyphWz-3Zg_e5euslN3");

static bool isWaylandSession()
{
    const QString sessionType = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    const QString waylandDisplay = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
    return !sessionType.isEmpty() && sessionType.compare("wayland", Qt::CaseInsensitive) == 0
        || !waylandDisplay.isEmpty();
}

class ScreenCaptureProvider : public QQuickImageProvider
{
public:
    explicit ScreenCaptureProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        if (isWaylandSession())
            return QPixmap();

        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen)
            return QPixmap();

        // Grab without coordinates to avoid multi-monitor issues
        QPixmap shot = screen->grabWindow(0);
        
        if (shot.isNull())
            return QPixmap();

        if (requestedSize.isValid())
            shot = shot.scaled(requestedSize, Qt::KeepAspectRatioByExpanding);

        if (size)
            *size = shot.size();

        return shot;
    }
};

int main(int argc, char *argv[])
{
    // Qt 5.15's qt_readlink() reads /etc/localtime into a 256-byte buffer in a
    // way that recent glibc's _FORTIFY_SOURCE aborts on. Populating TZ makes
    // QTimeZone skip that code path entirely.
    if (qEnvironmentVariableIsEmpty("TZ")) {
        QFile timezoneFile(QStringLiteral("/etc/timezone"));
        if (timezoneFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray zone = timezoneFile.readAll().trimmed();
            if (!zone.isEmpty())
                qputenv("TZ", zone);
        }
    }

#if CONCORD_HAS_GSTREAMER
    gst_init(&argc, &argv);
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Concord");
    QCoreApplication::setApplicationName("Concord");

    // All controllers below are parented to `app`/the engine's lifetime via
    // being declared on the stack in main() and outliving the engine, which
    // is destroyed before they go out of scope; none of them own resources
    // that need explicit early teardown except CallController (handled in
    // its destructor) and RealtimeClient (closes its socket in its dtor).
    SupabaseClient supabaseClient;
    supabaseClient.configure(kSupabaseUrl, kSupabaseAnonKey);

    RealtimeClient realtimeClient;

    AuthController authController(&supabaseClient);
    ServerController serverController(&supabaseClient, &realtimeClient);
    ChatController chatController(&supabaseClient, &realtimeClient);
    DirectMessageController directMessageController(&supabaseClient, &realtimeClient);
    SettingsController settingsController(&supabaseClient);
    CallController callController(&supabaseClient, &realtimeClient);
    CallPresenceController callPresenceController(&supabaseClient, &realtimeClient);
    OnlinePresenceController onlinePresenceController(&supabaseClient, &realtimeClient);
    ServerMembersController serverMembersController(&supabaseClient, &realtimeClient,
                                                     &onlinePresenceController);
    MediaDeviceController mediaDeviceController;

    QObject::connect(&authController, &AuthController::authenticatedChanged, [&]() {
        if (!authController.isAuthenticated()) {
            onlinePresenceController.clear();
            return;
        }
        realtimeClient.connectToServer(kSupabaseUrl, kSupabaseAnonKey, supabaseClient.accessToken());
        settingsController.syncFromRemote();

        const QString name = !authController.displayName().isEmpty() ? authController.displayName()
                                                                     : authController.username();
        onlinePresenceController.setLocalUser(authController.userId(), name);
    });

    QObject::connect(&authController, &AuthController::profileChanged, [&]() {
        const QString name = !authController.displayName().isEmpty() ? authController.displayName()
                                                                     : authController.username();
        if (!name.isEmpty())
            callController.setLocalDisplayName(name);
        directMessageController.setLocalDisplayName(name);
        if (authController.isAuthenticated())
            onlinePresenceController.setLocalUser(authController.userId(), name);
    });

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&]() {
        callController.shutdown();
    });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("screenpreview"), new ScreenCaptureProvider());

    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("screenCaptureSupported", !isWaylandSession());
    ctx->setContextProperty("supabaseClient", &supabaseClient);
    ctx->setContextProperty("realtimeClient", &realtimeClient);
    ctx->setContextProperty("authController", &authController);
    ctx->setContextProperty("serverController", &serverController);
    ctx->setContextProperty("serverMembers", &serverMembersController);
    ctx->setContextProperty("onlinePresence", &onlinePresenceController);
    ctx->setContextProperty("chatController", &chatController);
    ctx->setContextProperty("directMessageController", &directMessageController);
    ctx->setContextProperty("settingsController", &settingsController);
    ctx->setContextProperty("callController", &callController);
    ctx->setContextProperty("callPresence", &callPresenceController);
    ctx->setContextProperty("mediaDeviceController", &mediaDeviceController);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    const int result = app.exec();

    // Explicit, ordered shutdown to avoid GStreamer pipelines/sockets
    // outliving the Qt event loop.
    callController.leaveCall();
    onlinePresenceController.clear();
    realtimeClient.disconnectFromServer();
#if CONCORD_HAS_GSTREAMER
    gst_deinit();
#endif

    return result;
}
