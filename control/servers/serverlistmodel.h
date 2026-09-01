#ifndef SERVERLISTMODEL_H
#define SERVERLISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

struct ServerEntry {
    QString id;
    QString name;
    QString iconUrl;
    QString ownerId;
    QString inviteCode;
};

class ServerListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, IconUrlRole, OwnerIdRole, InviteCodeRole };

    explicit ServerListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setServers(const QJsonArray &rows);
    void upsertServer(const QJsonObject &row);
    QString serverIdAt(int row) const;
    const ServerEntry *serverById(const QString &serverId) const;

private:
    QVector<ServerEntry> m_servers;
};

#endif // SERVERLISTMODEL_H
