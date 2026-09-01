#include "authcontroller.h"

#include <QSettings>
#include <QCoreApplication>
#include <QJsonDocument>

AuthController::AuthController(SupabaseClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &SupabaseClient::signUpFinished, this, [this](bool ok, const QJsonObject &data, const QString &error) {
        setBusy(false);
        if (!ok) {
            setLastError(error);
            return;
        }
        emit signUpSucceeded();
    });

    connect(m_client, &SupabaseClient::signInFinished, this, [this](bool ok, const QJsonObject &data, const QString &error) {
        setBusy(false);
        if (!ok) {
            // GoTrue reports an unconfirmed e-mail via error_code/msg fields;
            // route the user to the confirmation screen instead of a plain error.
            const QJsonObject errorBody = QJsonDocument::fromJson(error.toUtf8()).object();
            const QString reason = errorBody.value("msg").toString() + errorBody.value("error_description").toString() + errorBody.value("error_code").toString();
            if (reason.contains("confirm", Qt::CaseInsensitive)) {
                emit emailNotConfirmed();
                return;
            }
            setLastError(error);
            return;
        }
        const QString accessToken = data.value("access_token").toString();
        const QString refreshToken = data.value("refresh_token").toString();
        const QString userId = data.value("user").toObject().value("id").toString();
        m_client->setSession(accessToken, refreshToken, userId);

        QSettings settings;
        settings.setValue("auth/refreshToken", refreshToken);
        settings.setValue("auth/userId", userId);

        emit authenticatedChanged();
        emit signInSucceeded();
        fetchProfile();
        m_pendingPassword.fill(QChar(0));
        m_pendingPassword.clear();
    });

    connect(m_client, &SupabaseClient::sessionRefreshed, this, [this](bool ok, const QJsonObject &data, const QString &error) {
        if (!ok) {
            setLastError(error);
            return;
        }
        const QString accessToken = data.value("access_token").toString();
        const QString refreshToken = data.value("refresh_token").toString();
        const QString userId = data.value("user").toObject().value("id").toString();
        m_client->setSession(accessToken, refreshToken, userId);

        QSettings settings;
        settings.setValue("auth/refreshToken", refreshToken);
        settings.setValue("auth/userId", userId);

        emit authenticatedChanged();
        fetchProfile();
    });

    connect(m_client, &SupabaseClient::signOutFinished, this, [this](bool, const QString &) {
        m_client->clearSession();
        m_username.clear();
        m_displayName.clear();
        m_pendingPassword.fill(QChar(0));
        m_pendingPassword.clear();

        QSettings settings;
        settings.remove("auth/refreshToken");
        settings.remove("auth/userId");

        emit authenticatedChanged();
        emit profileChanged();
    });

    connect(m_client, &SupabaseClient::selectFinished, this, [this](const QString &requestId, bool ok, const QJsonArray &rows, const QString &) {
        if (requestId != m_profileRequestId || !ok || rows.isEmpty())
            return;
        const QJsonObject profile = rows.first().toObject();
        m_username = profile.value("username").toString();
        m_displayName = profile.value("display_name").toString();
        emit profileChanged();
    });

    connect(m_client, &SupabaseClient::rpcFinished, this, [this](const QString &tag, bool ok, const QJsonValue &result, const QString &error) {
        if (tag != "auth:resolveUsername")
            return;
        if (!ok || !result.isString() || result.toString().isEmpty()) {
            setBusy(false);
            setLastError("Usuário ou senha inválidos");
            return;
        }
        m_pendingConfirmationEmail = result.toString();
        emit pendingConfirmationEmailChanged();
        m_client->signIn(m_pendingConfirmationEmail, m_pendingPassword);
    });

    connect(m_client, &SupabaseClient::resendConfirmationFinished, this, [this](bool ok, const QString &error) {
        if (!ok) {
            setLastError(error);
            return;
        }
        emit confirmationEmailResent();
    });
}

void AuthController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void AuthController::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

void AuthController::signUp(const QString &email, const QString &password, const QString &username, const QString &displayName)
{
    setBusy(true);
    setLastError(QString());
    m_pendingConfirmationEmail = email;
    emit pendingConfirmationEmailChanged();
    m_client->signUp(email, password, QJsonObject{{"username", username}, {"display_name", displayName}});
}

void AuthController::signIn(const QString &identifier, const QString &password)
{
    setBusy(true);
    setLastError(QString());
    m_pendingPassword = password;

    if (identifier.contains('@')) {
        m_pendingConfirmationEmail = identifier;
        emit pendingConfirmationEmailChanged();
        m_client->signIn(identifier, password);
    } else {
        m_client->rpc("get_email_for_username", QJsonObject{{"p_username", identifier}}, "auth:resolveUsername");
    }
}

void AuthController::resendConfirmationEmail()
{
    if (m_pendingConfirmationEmail.isEmpty())
        return;
    m_client->resendConfirmationEmail(m_pendingConfirmationEmail);
}

void AuthController::signOut()
{
    setBusy(true);
    m_client->signOut();
}

void AuthController::tryRestoreSession()
{
    QSettings settings;
    const QString refreshToken = settings.value("auth/refreshToken").toString();
    const QString userId = settings.value("auth/userId").toString();
    if (refreshToken.isEmpty() || userId.isEmpty())
        return;

    m_client->setSession(QString(), refreshToken, userId);
    m_client->refreshSession();
}

void AuthController::fetchProfile()
{
    // SupabaseClient::selectFinished is shared across the whole app; use a
    // fixed tag so we only react to our own profile query's reply.
    m_profileRequestId = QStringLiteral("auth-profile");
    m_client->select(QStringLiteral("profiles?id=eq.%1&select=*").arg(m_client->currentUserId()), m_profileRequestId);
}
