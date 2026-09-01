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
}

void CallParticipantModel::removeParticipant(const QString &userId)
{
    const int row = indexOf(userId);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_participants.removeAt(row);
    endRemoveRows();
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

void CallParticipantModel::clear()
{
    beginResetModel();
    m_participants.clear();
    endResetModel();
}
