#ifndef SERVERMEMBERMODEL_H
#define SERVERMEMBERMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct ServerMemberEntry {
    QString userId;
    QString username;
    QString displayName;
    QString avatarUrl;
    QString memberRole;
    QVariantList roles;
    bool online = false;
};

class ServerMemberModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        UserIdRole = Qt::UserRole + 1,
        UsernameRole,
        DisplayNameRole,
        AvatarUrlRole,
        MemberRoleRole,
        RolesRole,
        OnlineRole,
        HighestRoleNameRole,
        HighestRoleColorRole
    };

    explicit ServerMemberModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMembers(const QJsonArray &rows);
    void setOnlineUsers(const QVariantMap &onlineUsers);
    void clear();

    const QVector<ServerMemberEntry> &entries() const { return m_members; }
    const ServerMemberEntry *memberById(const QString &userId) const;

private:
    QVector<ServerMemberEntry> m_members;
    QVariantMap m_onlineUsers;
};

#endif // SERVERMEMBERMODEL_H
