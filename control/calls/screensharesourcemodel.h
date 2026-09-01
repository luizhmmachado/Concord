#ifndef SCREENSHARESOURCEMODEL_H
#define SCREENSHARESOURCEMODEL_H

#include <QAbstractListModel>
#include <QVector>

struct ScreenShareSource {
    enum Kind { Screen, Window };

    Kind kind = Screen;
    QString title;
    // Screen geometry, used to build the ximagesrc crop rectangle.
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    // X11 window id for Kind::Window.
    quint32 windowId = 0;
};

// Lists the monitors and top-level windows the user can share.
class ScreenShareSourceModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { TitleRole = Qt::UserRole + 1, KindRole, IsWindowRole };

    explicit ScreenShareSourceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    bool isValidIndex(int row) const { return row >= 0 && row < m_sources.size(); }
    const ScreenShareSource &at(int row) const { return m_sources.at(row); }

private:
    QVector<ScreenShareSource> m_sources;
};

#endif // SCREENSHARESOURCEMODEL_H
