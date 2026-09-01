#include "supabaseclient.h"

#include <QNetworkReply>
#include <QUuid>

SupabaseClient::SupabaseClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this)) // parented -> destroyed with this, no leak
{
}

void SupabaseClient::configure(const QString &projectUrl, const QString &anonKey)
{
    m_projectUrl = projectUrl;
    m_anonKey = anonKey;
}

void SupabaseClient::setSession(const QString &accessToken, const QString &refreshToken, const QString &userId)
{
    m_accessToken = accessToken;
    m_refreshToken = refreshToken;
    m_userId = userId;
}

void SupabaseClient::clearSession()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_userId.clear();
}

QNetworkRequest SupabaseClient::buildRequest(const QUrl &url, bool withAuth) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_anonKey.toUtf8());
    request.setRawHeader("Authorization", "Bearer " + (withAuth && !m_accessToken.isEmpty() ? m_accessToken.toUtf8() : m_anonKey.toUtf8()));
    return request;
}

QJsonArray SupabaseClient::parseArray(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isArray())
        return doc.array();
    if (doc.isObject())
        return QJsonArray{doc.object()};
    return {};
}

QJsonObject SupabaseClient::parseObject(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    return doc.isObject() ? doc.object() : QJsonObject();
}

void SupabaseClient::signUp(const QString &email, const QString &password, const QJsonObject &metadata)
{
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
    insert(table, QJsonArray{row}, tag, returning);
}

void SupabaseClient::insert(const QString &table, const QJsonArray &rows, const QString &tag, const QString &returning)
{
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

