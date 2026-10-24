#ifndef SERVERROLEMODEL_H
#define SERVERROLEMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
#include <QVector>

struct ServerRoleEntry {
    QString id;
    QString name;
    QString color;
    QVariantMap permissions;
    int position = 0;
};

class ServerRoleModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        RoleIdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        PermissionsRole,
        PositionRole
    };

    explicit ServerRoleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRoles(const QJsonArray &rows);
    void clear();
    const ServerRoleEntry *roleById(const QString &roleId) const;

private:
    QVector<ServerRoleEntry> m_roles;
};

#endif // SERVERROLEMODEL_H
