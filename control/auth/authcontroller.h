#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include <QObject>
#include "control/database/supabaseclient.h"

// Exposes sign up / sign in / sign out to QML and keeps the current session.
class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY authenticatedChanged)
    Q_PROPERTY(QString username READ username NOTIFY profileChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY profileChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString pendingConfirmationEmail READ pendingConfirmationEmail NOTIFY pendingConfirmationEmailChanged)

public:
    explicit AuthController(SupabaseClient *client, QObject *parent = nullptr);

    bool isAuthenticated() const { return m_client->isAuthenticated(); }
    QString userId() const { return m_client->currentUserId(); }
    QString username() const { return m_username; }
    QString displayName() const { return m_displayName; }
    bool busy() const { return m_busy; }
    QString lastError() const { return m_lastError; }
    QString pendingConfirmationEmail() const { return m_pendingConfirmationEmail; }

    Q_INVOKABLE void signUp(const QString &email, const QString &password, const QString &username, const QString &displayName);
    // Accepts either an e-mail address or a username as `identifier`.
    Q_INVOKABLE void signIn(const QString &identifier, const QString &password);
    Q_INVOKABLE void signOut();
    // Retries the sign in with the credentials kept from the sign up/sign in
    // attempt, so the user lands straight in the app after confirming.
    Q_INVOKABLE void confirmSignIn();
    Q_INVOKABLE void tryRestoreSession();
    Q_INVOKABLE void resendConfirmationEmail();

signals:
    void authenticatedChanged();
    void profileChanged();
    void busyChanged();
    void lastErrorChanged();
    void pendingConfirmationEmailChanged();
    void signUpSucceeded();
    void signInSucceeded();
    void confirmationEmailResent();
    void emailNotConfirmed();

private:
    void setBusy(bool busy);
    void setLastError(const QString &error);
    void fetchProfile();

    SupabaseClient *m_client;
    QString m_username;
    QString m_displayName;
    bool m_busy = false;
    QString m_lastError;
    QString m_profileRequestId;
    QString m_pendingConfirmationEmail;
    QString m_pendingPassword;
};

#endif // AUTHCONTROLLER_H
