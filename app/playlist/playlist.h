#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include "track.h"
#include "playlist/sorter.h"

#include <QString>
#include <QDir>
#include <QVector>
#include <QObject>
#include <QFuture>
#include <QMutex>

namespace Playlist {
  class Playlist {
  public:
    enum PlaylistRandom {
      None = 0,
      Random,
      Sequential,
      SequentialNoLoop
    };

    explicit Playlist();

    QString name() const;
    QString rename(const QString &value);
    QVector<Track> tracks() const;

    void load(const QVector<Track> &tracks);
    void append(const QVector<Track> &tracks, bool with_sort);

    quint64 uid() const;

    // The Favorites playlist is maintained by the app, not by the user: it is
    // created on the first favorite, cannot be renamed or removed, and the
    // heart state of every track is derived from its contents.
    bool favorites() const;
    void setFavorites(bool value);

    bool hasTrack(quint64 track_uid) const;
    int trackIndex(quint64 track_uid) const;
    Track trackBy(quint64 uid) const;

    void removeTrack(int position);

    enum PlaylistRandom random() const;
    void setRandom(enum PlaylistRandom arg);

    void sortBy(const QString &criteria);

    QByteArray toM3U() const;

    void reload();
    void reloadTrack(quint64 uid);

  private:
    QVector<Track> sort(QVector<Track> list, const Sorter &sorter = Sorter());

    QString playlist_name;
    QVector<Track> tracks_list;
    quint64 _uid;
    enum PlaylistRandom _random;
    bool _favorites = false;
    mutable QMutex mutex;
  };
}

#endif // PLAYLISTITEM_H
