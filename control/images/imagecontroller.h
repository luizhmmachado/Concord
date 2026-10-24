#ifndef IMAGECONTROLLER_H
#define IMAGECONTROLLER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Owns the in-app image viewer state and downloads the currently viewed image
// to the user's Downloads folder.
class ImageController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString viewerUrl READ viewerUrl NOTIFY viewerUrlChanged)

public:
    explicit ImageController(QObject *parent = nullptr);

    QString viewerUrl() const { return m_viewerUrl; }

    Q_INVOKABLE void openImage(const QString &url);
    Q_INVOKABLE void closeImage();
    Q_INVOKABLE void downloadImage(const QString &url);

signals:
    void viewerUrlChanged();
    void downloadFinished(bool success, const QString &path, const QString &error);

private:
    QString downloadPathFor(const QString &urlString) const;

    QNetworkAccessManager *m_manager;
    QString m_viewerUrl;
};

#endif // IMAGECONTROLLER_H
