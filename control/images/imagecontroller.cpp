#include "imagecontroller.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <QDebug>

ImageController::ImageController(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    qInfo().noquote() << "ImageController::ImageController";
}

void ImageController::openImage(const QString &url)
{
    qInfo().noquote() << "ImageController::openImage" << "url=" << url;
    if (url.isEmpty() || url == m_viewerUrl)
        return;
    m_viewerUrl = url;
    emit viewerUrlChanged();
}

void ImageController::closeImage()
{
    qInfo().noquote() << "ImageController::closeImage";
    if (m_viewerUrl.isEmpty())
        return;
    m_viewerUrl.clear();
    emit viewerUrlChanged();
}

QString ImageController::downloadPathFor(const QString &urlString) const
{
    QString suffix = QFileInfo(QUrl(urlString).path()).suffix().toLower();
    if (suffix.isEmpty() || suffix.size() > 8)
        suffix = QStringLiteral("png");

    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (dirPath.isEmpty())
        dirPath = QDir::homePath();

    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return {};

    const QString base = QStringLiteral("concord-imagem-%1")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    QString path = dir.filePath(base + QStringLiteral(".") + suffix);
    int counter = 2;
    while (QFileInfo::exists(path))
        path = dir.filePath(QStringLiteral("%1 (%2).%3").arg(base).arg(counter++).arg(suffix));
    return path;
}

void ImageController::downloadImage(const QString &urlString)
{
    qInfo().noquote() << "ImageController::downloadImage" << "url=" << urlString;
    const QUrl url(urlString);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        emit downloadFinished(false, QString(), QStringLiteral("URL de imagem inválida."));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "Concord");

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, urlString]() {
        const QByteArray data = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError
                        && status >= 200 && status < 300 && !data.isEmpty();
        if (!ok) {
            QString error = reply->errorString();
            if (status >= 400)
                error = QStringLiteral("HTTP %1").arg(status);
            emit downloadFinished(false, QString(), error);
            reply->deleteLater();
            return;
        }

        const QString path = downloadPathFor(urlString);
        if (path.isEmpty()) {
            emit downloadFinished(false, QString(), QStringLiteral("Não foi possível criar o arquivo de destino."));
            reply->deleteLater();
            return;
        }

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFinished(false, QString(), QStringLiteral("Não foi possível salvar a imagem."));
            reply->deleteLater();
            return;
        }
        if (file.write(data) != data.size()) {
            file.cancelWriting();
            emit downloadFinished(false, QString(), QStringLiteral("Não foi possível salvar a imagem."));
            reply->deleteLater();
            return;
        }
        if (!file.commit()) {
            emit downloadFinished(false, QString(), QStringLiteral("Não foi possível salvar a imagem."));
            reply->deleteLater();
            return;
        }

        emit downloadFinished(true, path, QString());
        reply->deleteLater();
    });
}
