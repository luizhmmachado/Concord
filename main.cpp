#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <gst/gst.h>

#include "control/database/supabaseclient.h"
#include "control/database/realtimeclient.h"
#include "control/auth/authcontroller.h"
#include "control/servers/servercontroller.h"
#include "control/chat/chatcontroller.h"
#include "control/settings/settingscontroller.h"
#include "control/calls/callcontroller.h"
#include "control/calls/mediadevicecontroller.h"

// TODO: move these to a local, git-ignored config file before shipping;
// they are safe to ship only because the anon key is meant to be public
// and all data access is protected by Postgres Row Level Security (see
// supabase/migrations/0002_row_level_security.sql).
static const QString kSupabaseUrl = QStringLiteral("https://vwhkcthqvofyadjforcf.supabase.co");
static const QString kSupabaseAnonKey = QStringLiteral("sb_publishable_Qg9GqLbfC4kttyphWz-3Zg_e5euslN3");

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

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
    SettingsController settingsController(&supabaseClient);
    CallController callController(&supabaseClient, &realtimeClient);
    MediaDeviceController mediaDeviceController;

    QObject::connect(&authController, &AuthController::authenticatedChanged, [&]() {
        if (!authController.isAuthenticated())
            return;
        realtimeClient.connectToServer(kSupabaseUrl, kSupabaseAnonKey, supabaseClient.accessToken());
        settingsController.syncFromRemote();
    });

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty("supabaseClient", &supabaseClient);
    ctx->setContextProperty("realtimeClient", &realtimeClient);
    ctx->setContextProperty("authController", &authController);
    ctx->setContextProperty("serverController", &serverController);
    ctx->setContextProperty("chatController", &chatController);
    ctx->setContextProperty("settingsController", &settingsController);
    ctx->setContextProperty("callController", &callController);
    ctx->setContextProperty("mediaDeviceController", &mediaDeviceController);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    const int result = app.exec();

    // Explicit, ordered shutdown to avoid GStreamer pipelines/sockets
    // outliving the Qt event loop.
    callController.leaveCall();
    realtimeClient.disconnectFromServer();
    gst_deinit();

    return result;
}
