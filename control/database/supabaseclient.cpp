#include "supabaseclient.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QUrl>
#include <QUuid>

#include <QDebug>
SupabaseClient::SupabaseClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this)) // parented -> destroyed with this, no leak
{
    qInfo().noquote() << "SupabaseClient::SupabaseClient";
}

void SupabaseClient::configure(const QString &projectUrl, const QString &anonKey)
{
    qInfo().noquote() << "SupabaseClient::configure" << "projectUrl=" << projectUrl;
    m_projectUrl = projectUrl;
    m_anonKey = anonKey;
}

void SupabaseClient::setSession(const QString &accessToken, const QString &refreshToken, const QString &userId)
{
    qInfo().noquote() << "SupabaseClient::setSession" << "userId=" << userId;
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_userId = userId;
}

void SupabaseClient::clearSession()
{
    qInfo().noquote() << "SupabaseClient::clearSession";
    m_accessToken.clear();
    m_refreshToken.clear();
    m_userId.clear();
}

QNetworkRequest SupabaseClient::buildRequest(const QUrl &url, bool withAuth) const
{
    qInfo().noquote() << "SupabaseClient::buildRequest" << "url=" << url << "withAuth=" << withAuth;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_anonKey.toUtf8());
    request.setRawHeader("Authorization", "Bearer " + (withAuth && !m_accessToken.isEmpty() ? m_accessToken.toUtf8() : m_anonKey.toUtf8()));
    return request;
}

QJsonArray SupabaseClient::parseArray(const QByteArray &body)
{
    qInfo().noquote() << "SupabaseClient::parseArray" << "body=" << body;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isArray())
        return doc.array();
    if (doc.isObject())
        return QJsonArray{doc.object()};
    return {};
}

QJsonObject SupabaseClient::parseObject(const QByteArray &body)
{
    qInfo().noquote() << "SupabaseClient::parseObject" << "body=" << body;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    return doc.isObject() ? doc.object() : QJsonObject();
}

void SupabaseClient::signUp(const QString &email, const QString &password, const QJsonObject &metadata)
{
    qInfo().noquote() << "SupabaseClient::signUp" << "email=" << email << "metadata=" << metadata;
    QUrl url(m_projectUrl + "/auth/v1/signup");
    QNetworkRequest request = buildRequest(url, false);

    QJsonObject body{{"email", email}, {"password", password}};
    if (!metadata.isEmpty())
        body["data"] = metadata;

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit signUpFinished(ok, parseObject(data), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::signIn(const QString &email, const QString &password)
{
    qInfo().noquote() << "SupabaseClient::signIn" << "email=" << email;
    QUrl url(m_projectUrl + "/auth/v1/token");
    QUrlQuery query;
    query.addQueryItem("grant_type", "password");
    url.setQuery(query);
    QNetworkRequest request = buildRequest(url, false);

    const QJsonObject body{{"email", email}, {"password", password}};
    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit signInFinished(ok, parseObject(data), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::signOut()
{
    qInfo().noquote() << "SupabaseClient::signOut";
    QUrl url(m_projectUrl + "/auth/v1/logout");
    QNetworkRequest request = buildRequest(url, true);
    QNetworkReply *reply = m_manager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit signOutFinished(ok, ok ? QString() : QString::fromUtf8(reply->readAll()));
        reply->deleteLater();
    });
}

void SupabaseClient::refreshSession()
{
    qInfo().noquote() << "SupabaseClient::refreshSession";
    QUrl url(m_projectUrl + "/auth/v1/token");
    QUrlQuery query;
    query.addQueryItem("grant_type", "refresh_token");
    url.setQuery(query);
    QNetworkRequest request = buildRequest(url, false);

    const QJsonObject body{{"refresh_token", m_refreshToken}};
    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit sessionRefreshed(ok, parseObject(data), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::resendConfirmationEmail(const QString &email)
{
    qInfo().noquote() << "SupabaseClient::resendConfirmationEmail" << "email=" << email;
    QUrl url(m_projectUrl + "/auth/v1/resend");
    QNetworkRequest request = buildRequest(url, false);

    const QJsonObject body{{"type", "signup"}, {"email", email}};
    QNetworkReply *reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit resendConfirmationFinished(ok, ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::select(const QString &tableAndQuery, const QString &tag)
{
    qInfo().noquote() << "SupabaseClient::select" << "tableAndQuery=" << tableAndQuery << "tag=" << tag;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;
    QUrl url(m_projectUrl + "/rest/v1/" + tableAndQuery);
    QNetworkRequest request = buildRequest(url, true);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit selectFinished(requestId, ok, ok ? parseArray(data) : QJsonArray(), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::insert(const QString &table, const QJsonObject &row, const QString &tag, const QString &returning)
{
    qInfo().noquote() << "SupabaseClient::insert" << "table=" << table << "row=" << row << "tag=" << tag << "returning=" << returning;
    insert(table, QJsonArray{row}, tag, returning);
}

void SupabaseClient::insert(const QString &table, const QJsonArray &rows, const QString &tag, const QString &returning)
{
    qInfo().noquote() << "SupabaseClient::insert" << "table=" << table << "rows=" << rows << "tag=" << tag << "returning=" << returning;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;
    QUrl url(m_projectUrl + "/rest/v1/" + table);
    QNetworkRequest request = buildRequest(url, true);
    request.setRawHeader("Prefer", ("return=" + returning).toUtf8());

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(rows).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit insertFinished(requestId, ok, ok ? parseArray(data) : QJsonArray(), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::update(const QString &tableAndQuery, const QJsonObject &patch, const QString &tag)
{
    qInfo().noquote() << "SupabaseClient::update" << "tableAndQuery=" << tableAndQuery << "patch=" << patch << "tag=" << tag;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;
    QUrl url(m_projectUrl + "/rest/v1/" + tableAndQuery);
    QNetworkRequest request = buildRequest(url, true);
    request.setRawHeader("Prefer", "return=representation");

    QNetworkReply *reply = m_manager->sendCustomRequest(request, "PATCH", QJsonDocument(patch).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit updateFinished(requestId, ok, ok ? parseArray(data) : QJsonArray(), ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::remove(const QString &tableAndQuery, const QString &tag)
{
    qInfo().noquote() << "SupabaseClient::remove" << "tableAndQuery=" << tableAndQuery << "tag=" << tag;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;
    QUrl url(m_projectUrl + "/rest/v1/" + tableAndQuery);
    QNetworkRequest request = buildRequest(url, true);

    QNetworkReply *reply = m_manager->sendCustomRequest(request, "DELETE");
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit deleteFinished(requestId, ok, ok ? QString() : QString::fromUtf8(reply->readAll()));
        reply->deleteLater();
    });
}

void SupabaseClient::rpc(const QString &functionName, const QJsonObject &params, const QString &tag)
{
    qInfo().noquote() << "SupabaseClient::rpc" << "functionName=" << functionName << "params=" << params << "tag=" << tag;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;
    QUrl url(m_projectUrl + "/rest/v1/rpc/" + functionName);
    QNetworkRequest request = buildRequest(url, true);

    QNetworkReply *reply = m_manager->post(request, QJsonDocument(params).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray data = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        QJsonValue result;
        if (ok) {
            // PostgREST returns a bare JSON scalar/array/object for RPC calls;
            // wrap it in an array so QJsonDocument can parse scalar replies too.
            const QJsonDocument wrapped = QJsonDocument::fromJson("[" + data + "]");
            result = (wrapped.isArray() && !wrapped.array().isEmpty()) ? wrapped.array().first() : QJsonValue();
        }
        emit rpcFinished(requestId, ok, result, ok ? QString() : QString::fromUtf8(data));
        reply->deleteLater();
    });
}

void SupabaseClient::uploadChatImage(const QString &localFilePath, const QString &tag)
{
    qInfo().noquote() << "SupabaseClient::uploadChatImage" << "localFilePath=" << localFilePath << "tag=" << tag;
    const QString requestId = tag.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : tag;

    if (m_userId.isEmpty() || m_accessToken.isEmpty()) {
        emit imageUploadFinished(requestId, false, QString(), QStringLiteral("Sessão expirada."));
        return;
    }

    QString localPath = localFilePath;
    const QUrl source(localFilePath);
    if (source.isLocalFile())
        localPath = source.toLocalFile();

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit imageUploadFinished(requestId, false, QString(),
                                 QStringLiteral("Não foi possível abrir a imagem selecionada."));
        return;
    }

    constexpr qint64 maxImageBytes = 10 * 1024 * 1024;
    if (file.size() <= 0 || file.size() > maxImageBytes) {
        emit imageUploadFinished(requestId, false, QString(),
                                 QStringLiteral("A imagem deve ter no máximo 10 MB."));
        return;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(localPath, QMimeDatabase::MatchContent);
    if (!mime.name().startsWith("image/")) {
        emit imageUploadFinished(requestId, false, QString(),
                                 QStringLiteral("Selecione um arquivo de imagem."));
        return;
    }

    QString suffix = mime.preferredSuffix().toLower();
    if (suffix.isEmpty())
        suffix = QFileInfo(localPath).suffix().toLower();
    const QString objectPath = m_userId + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces)
                               + (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix);

    QUrl url(m_projectUrl + "/storage/v1/object/chat-images/" + objectPath);
    QNetworkRequest request(url);
    request.setRawHeader("apikey", m_anonKey.toUtf8());
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, mime.name());
    request.setRawHeader("x-upsert", "false");
    request.setRawHeader("cache-control", "3600");

    const QByteArray data = file.readAll();
    QNetworkReply *reply = m_manager->post(request, data);
    const QString publicUrl = m_projectUrl + "/storage/v1/object/public/chat-images/" + objectPath;
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId, publicUrl]() {
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError;
        QString error;
        if (!ok) {
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            error = obj.value("message").toString();
            if (error.isEmpty())
                error = QString::fromUtf8(body);
            if (error.isEmpty())
                error = reply->errorString();
        }
        emit imageUploadFinished(requestId, ok, ok ? publicUrl : QString(), error);
        reply->deleteLater();
    });
}
