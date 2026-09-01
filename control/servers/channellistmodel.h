#ifndef CHANNELLISTMODEL_H
#define CHANNELLISTMODEL_H

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>

struct ChannelEntry {
    QString id;
    QString serverId;
    QString name;
    QString type; // "text" or "voice"
    int position = 0;
};

class ChannelListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, ServerIdRole, NameRole, TypeRole, PositionRole };

    explicit ChannelListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setChannels(const QJsonArray &rows);
    void addChannel(const QJsonObject &row);
    QString channelIdAt(int row) const;
    QString channelTypeAt(int row) const;

private:
    QVector<ChannelEntry> m_channels;
};

#endif // CHANNELLISTMODEL_H
