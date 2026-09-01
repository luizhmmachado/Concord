#ifndef SUPABASECLIENT_H
#define SUPABASECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QUrlQuery>

// Thin wrapper around Supabase's REST (PostgREST) and GoTrue (auth) HTTP APIs.
// All QNetworkReply objects are cleaned up with deleteLater() as soon as they
// finish to avoid leaking network resources.
class SupabaseClient : public QObject
{
    Q_OBJECT
public:
    explicit SupabaseClient(QObject *parent = nullptr);

    void configure(const QString &projectUrl, const QString &anonKey);

    QString projectUrl() const { return m_projectUrl; }
    QString anonKey() const { return m_anonKey; }
    QString accessToken() const { return m_accessToken; }

    void setSession(const QString &accessToken, const QString &refreshToken, const QString &userId);
    void clearSession();
    bool isAuthenticated() const { return !m_accessToken.isEmpty(); }
    QString currentUserId() const { return m_userId; }
    QString refreshToken() const { return m_refreshToken; }

    // GoTrue auth endpoints
    void signUp(const QString &email, const QString &password, const QJsonObject &metadata);
    void signIn(const QString &email, const QString &password);
    void signOut();
    void refreshSession();
    void resendConfirmationEmail(const QString &email);

    // Generic PostgREST helpers ("table" e.g. "servers?select=*").
    // "tag" is echoed back in the corresponding *Finished signal so callers
    // can tell which of their in-flight requests just completed; if left
    // empty a random id is generated instead.
    void select(const QString &tableAndQuery, const QString &tag = QString());
    void insert(const QString &table, const QJsonObject &row, const QString &tag = QString(), const QString &returning = "representation");
    void insert(const QString &table, const QJsonArray &rows, const QString &tag = QString(), const QString &returning = "representation");
    void update(const QString &tableAndQuery, const QJsonObject &patch, const QString &tag = QString());
    void remove(const QString &tableAndQuery, const QString &tag = QString());

    // Calls a PostgREST RPC endpoint (Postgres function exposed via `/rest/v1/rpc/<name>`).
    void rpc(const QString &functionName, const QJsonObject &params, const QString &tag = QString());

signals:
    void signUpFinished(bool success, const QJsonObject &data, const QString &error);
    void signInFinished(bool success, const QJsonObject &data, const QString &error);
    void signOutFinished(bool success, const QString &error);
    void sessionRefreshed(bool success, const QJsonObject &data, const QString &error);
    void resendConfirmationFinished(bool success, const QString &error);

    void selectFinished(const QString &requestId, bool success, const QJsonArray &rows, const QString &error);
    void insertFinished(const QString &requestId, bool success, const QJsonArray &rows, const QString &error);
    void updateFinished(const QString &requestId, bool success, const QJsonArray &rows, const QString &error);
    void deleteFinished(const QString &requestId, bool success, const QString &error);
    void rpcFinished(const QString &requestId, bool success, const QJsonValue &result, const QString &error);

private:
    QNetworkRequest buildRequest(const QUrl &url, bool withAuth = true) const;
    static QJsonArray parseArray(const QByteArray &body);
    static QJsonObject parseObject(const QByteArray &body);

    QNetworkAccessManager *m_manager;
    QString m_projectUrl;
    QString m_anonKey;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_userId;
};

#endif // SUPABASECLIENT_H
