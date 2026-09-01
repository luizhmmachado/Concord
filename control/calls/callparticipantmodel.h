#ifndef CALLPARTICIPANTMODEL_H
#define CALLPARTICIPANTMODEL_H

#include <QAbstractListModel>

struct CallParticipantEntry {
    QString userId;
    QString displayName;
    bool muted = false;
    bool cameraOn = false;
    bool screenSharing = false;
    QString connectionState = "new";
    QObject *videoSource = nullptr;
    QObject *screenSource = nullptr;
};

class CallParticipantModel : public QAbstractListModel
{
    Q_OBJECT
    // QML cannot react to rowCount(); expose it as a notifying property.
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles { UserIdRole = Qt::UserRole + 1, DisplayNameRole, MutedRole, CameraOnRole, ScreenSharingRole, ConnectionStateRole, VideoSourceRole, ScreenSourceRole };

    explicit CallParticipantModel(QObject *parent = nullptr);

    int count() const { return m_participants.size(); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addParticipant(const QString &userId, const QString &displayName);
    void setDisplayName(const QString &userId, const QString &displayName);
    void removeParticipant(const QString &userId);
    void setConnectionState(const QString &userId, const QString &state);
    void setMuted(const QString &userId, bool muted);
    void setCameraOn(const QString &userId, bool cameraOn);
    void setScreenSharing(const QString &userId, bool sharing);
    void setVideoSource(const QString &userId, QObject *source);
    void setScreenSource(const QString &userId, QObject *source);
    void clear();

    bool isScreenSharing(const QString &userId) const;

signals:
    void countChanged();

private:
    int indexOf(const QString &userId) const;

    QVector<CallParticipantEntry> m_participants;
};

#endif // CALLPARTICIPANTMODEL_H
