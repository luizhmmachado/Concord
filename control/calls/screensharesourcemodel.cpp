#include "screensharesourcemodel.h"

#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

#if defined(Q_OS_LINUX)
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
#endif

ScreenShareSourceModel::ScreenShareSourceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    qInfo().noquote() << "ScreenShareSourceModel::ScreenShareSourceModel";
    refresh();
}

int ScreenShareSourceModel::rowCount(const QModelIndex &parent) const
{
    qInfo().noquote() << "ScreenShareSourceModel::rowCount";
    if (parent.isValid())
        return 0;
    return m_sources.size();
}

QVariant ScreenShareSourceModel::data(const QModelIndex &index, int role) const
{
    qInfo().noquote() << "ScreenShareSourceModel::data" << "index=" << index << "role=" << role;
    if (!index.isValid() || index.row() >= m_sources.size())
        return {};

    const ScreenShareSource &source = m_sources.at(index.row());
    switch (role) {
    case TitleRole: return source.title;
    case KindRole: return source.kind == ScreenShareSource::Window ? "window" : "screen";
    case IsWindowRole: return source.kind == ScreenShareSource::Window;
    default: return {};
    }
}

QHash<int, QByteArray> ScreenShareSourceModel::roleNames() const
{
    qInfo().noquote() << "ScreenShareSourceModel::roleNames";
    return {
        {TitleRole, "title"},
        {KindRole, "kind"},
        {IsWindowRole, "isWindow"},
    };
}

#if defined(Q_OS_LINUX)
namespace {

xcb_atom_t internAtom(xcb_connection_t *conn, const char *name)
{
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 1, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookie, nullptr);
    const xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply);
    return atom;
}

QString windowTitle(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t netWmName, xcb_atom_t utf8String)
{
    auto fetch = [&](xcb_atom_t property, xcb_atom_t type) -> QString {
        xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0, window, property, type, 0, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, nullptr);
        if (!reply)
            return {};
        const int length = xcb_get_property_value_length(reply);
        QString value;
        if (length > 0)
            value = QString::fromUtf8(static_cast<const char *>(xcb_get_property_value(reply)), length);
        free(reply);
        return value;
    };

    QString title = fetch(netWmName, utf8String);
    if (title.isEmpty())
        title = fetch(XCB_ATOM_WM_NAME, XCB_ATOM_STRING);
    return title;
}

} // namespace
#endif

void ScreenShareSourceModel::refresh()
{
    qInfo().noquote() << "ScreenShareSourceModel::refresh";
    beginResetModel();
    m_sources.clear();

    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);
        const QRect geometry = screen->geometry();
        ScreenShareSource source;
        source.kind = ScreenShareSource::Screen;
        source.title = screens.size() > 1
                           ? QStringLiteral("Tela %1 (%2) — %3x%4")
                                 .arg(i + 1)
                                 .arg(screen->name())
                                 .arg(geometry.width())
                                 .arg(geometry.height())
                           : QStringLiteral("Tela inteira — %1x%2").arg(geometry.width()).arg(geometry.height());
        source.x = geometry.x();
        source.y = geometry.y();
        source.width = geometry.width();
        source.height = geometry.height();
        m_sources.append(source);
    }

#if defined(Q_OS_LINUX)
    int screenNumber = 0;
    xcb_connection_t *conn = xcb_connect(nullptr, &screenNumber);
    if (conn && !xcb_connection_has_error(conn)) {
        const xcb_setup_t *setup = xcb_get_setup(conn);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        for (int i = 0; i < screenNumber && it.rem; ++i)
            xcb_screen_next(&it);

        const xcb_atom_t clientList = internAtom(conn, "_NET_CLIENT_LIST");
        const xcb_atom_t netWmName = internAtom(conn, "_NET_WM_NAME");
        const xcb_atom_t utf8String = internAtom(conn, "UTF8_STRING");

        if (it.rem && clientList != XCB_ATOM_NONE) {
            xcb_get_property_cookie_t cookie =
                xcb_get_property(conn, 0, it.data->root, clientList, XCB_ATOM_WINDOW, 0, 4096);
            xcb_get_property_reply_t *reply = xcb_get_property_reply(conn, cookie, nullptr);
            if (reply) {
                const int count = xcb_get_property_value_length(reply) / sizeof(xcb_window_t);
                const auto *windows = static_cast<xcb_window_t *>(xcb_get_property_value(reply));
                for (int i = 0; i < count; ++i) {
                    const QString title = windowTitle(conn, windows[i], netWmName, utf8String);
                    if (title.isEmpty())
                        continue;
                    ScreenShareSource source;
                    source.kind = ScreenShareSource::Window;
                    source.title = title;
                    source.windowId = windows[i];
                    m_sources.append(source);
                }
                free(reply);
            }
        }
    }
    if (conn)
        xcb_disconnect(conn);
#endif

    endResetModel();
}
