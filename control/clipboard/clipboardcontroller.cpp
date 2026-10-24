#include "clipboardcontroller.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMimeType>
#include <QTimer>
#include <QUuid>
#include <QUrl>

#include <QDebug>

ClipboardController::ClipboardController(QObject *parent)
    : QObject(parent)
{
    qInfo().noquote() << "ClipboardController::ClipboardController";
}

QString ClipboardController::pastedImagePath()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mime = clipboard->mimeData();

    // Prefer a copied local image file so the original quality is preserved.
    if (mime && mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (!url.isLocalFile())
                continue;
            const QString path = url.toLocalFile();
            const QMimeType type = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent);
            if (type.name().startsWith("image/"))
                return path;
        }
    }

    QImage image = clipboard->image();
    if (image.isNull() && mime && mime->hasImage())
        image = qvariant_cast<QImage>(mime->imageData());

    if (image.isNull())
        return {};

    constexpr int maxClipboardDimension = 2560;
    if (image.width() > maxClipboardDimension || image.height() > maxClipboardDimension) {
        image = image.scaled(maxClipboardDimension, maxClipboardDimension,
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QString path = QDir::tempPath()
                         + QStringLiteral("/concord-paste-%1.png")
                               .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!image.save(path, "PNG")) {
        qWarning().noquote() << "ClipboardController::pastedImagePath" << "failed to save" << path;
        return {};
    }
    QTimer::singleShot(60000, this, [path]() { QFile::remove(path); });
    return path;
}
