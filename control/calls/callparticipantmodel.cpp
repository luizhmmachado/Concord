#include "callparticipantmodel.h"

CallParticipantModel::CallParticipantModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CallParticipantModel::indexOf(const QString &userId) const
{
    for (int i = 0; i < m_participants.size(); ++i) {
        if (m_participants.at(i).userId == userId)
            return i;
    }
    return -1;
}

int CallParticipantModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_participants.size();
}

QVariant CallParticipantModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_participants.size())
        return {};

    const CallParticipantEntry &entry = m_participants.at(index.row());
    switch (role) {
    case UserIdRole: return entry.userId;
    case DisplayNameRole: return entry.displayName;
    case MutedRole: return entry.muted;
    case CameraOnRole: return entry.cameraOn;
    case ScreenSharingRole: return entry.screenSharing;
    case ConnectionStateRole: return entry.connectionState;
    case VideoSourceRole: return QVariant::fromValue(entry.videoSource);
    case ScreenSourceRole: return QVariant::fromValue(entry.screenSource);
    default: return {};
    }
}

QHash<int, QByteArray> CallParticipantModel::roleNames() const
{
    return {
        {UserIdRole, "userId"},
        {DisplayNameRole, "displayName"},
        {MutedRole, "muted"},
        {CameraOnRole, "cameraOn"},
        {ScreenSharingRole, "screenSharing"},
        {ConnectionStateRole, "connectionState"},
        {VideoSourceRole, "videoSource"},
        {ScreenSourceRole, "screenSource"},
    };
}

void CallParticipantModel::addParticipant(const QString &userId, const QString &displayName)
{
    if (indexOf(userId) >= 0)
        return;
    beginInsertRows(QModelIndex(), m_participants.size(), m_participants.size());
    CallParticipantEntry entry;
    entry.userId = userId;
    entry.displayName = displayName;
    m_participants.append(entry);
    endInsertRows();
    emit countChanged();
}

void CallParticipantModel::setDisplayName(const QString &userId, const QString &displayName)
{
    const int row = indexOf(userId);
    if (row < 0 || displayName.isEmpty() || m_participants.at(row).displayName == displayName)
        return;
    m_participants[row].displayName = displayName;
    emit dataChanged(index(row), index(row), {DisplayNameRole});
}

void CallParticipantModel::removeParticipant(const QString &userId)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_participants.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void CallParticipantModel::setConnectionState(const QString &userId, const QString &state)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].connectionState = state;
    emit dataChanged(index(row), index(row), {ConnectionStateRole});
}

void CallParticipantModel::setMuted(const QString &userId, bool muted)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].muted = muted;
    emit dataChanged(index(row), index(row), {MutedRole});
}

void CallParticipantModel::setCameraOn(const QString &userId, bool cameraOn)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].cameraOn = cameraOn;
    emit dataChanged(index(row), index(row), {CameraOnRole});
}

void CallParticipantModel::setScreenSharing(const QString &userId, bool sharing)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].screenSharing = sharing;
    emit dataChanged(index(row), index(row), {ScreenSharingRole});
}

void CallParticipantModel::setVideoSource(const QString &userId, QObject *source)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].videoSource = source;
    emit dataChanged(index(row), index(row), {VideoSourceRole});
}

void CallParticipantModel::setScreenSource(const QString &userId, QObject *source)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    m_participants[row].screenSource = source;
    emit dataChanged(index(row), index(row), {ScreenSourceRole});
}

bool CallParticipantModel::isScreenSharing(const QString &userId) const
{
    const int row = indexOf(userId);
    return row >= 0 && m_participants.at(row).screenSharing;
}

void CallParticipantModel::clear()
{
    beginResetModel();
    m_participants.clear();
    endResetModel();
    emit countChanged();
}
