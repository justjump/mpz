#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "playlistmodel.h"
#include "playlist/playlist.h"
#include "track.h"
#include "config/local.h"
#include "config/global.h"
#include "playlistproxyfiltermodel.h"
#include "playlist_ui/playlistcontextmenu.h"
#include "backgroundtasks.h"
#include "playlist_ui/columnsconfig.h"
#include "tracksmimedata.h"

#include <QObject>
#include <QTableView>
#include <memory>
#include <QDropEvent>
#include <QEvent>
#include <QLineEdit>
#include <QHash>
#include <QDropEvent>

namespace PlaylistUi {
  class Controller : public QObject {
    Q_OBJECT
  public:
    explicit Controller(QTableView *v, QLineEdit *search, BackgroundTasks *_tasks, Config::Local &local_cfg, Config::Global &global_cfg, ModusOperandi &modus, QObject *parent = nullptr);

    QVector<Track> currentTracks() const;
    QVector<Track> selectedTracks() const;
    // Selects and centers the track with this file path, if the current filter
    // still shows it. Used after jumping to a playlist from a global search.
    void selectTrack(const QString &path);
    // Heart state shown in the last column, owned by the Favorites playlist.
    void setFavoritePaths(const QSet<QString> &paths);
    // Selects every visible row whose file path is in paths. Used to keep the
    // tracks whose file could not be deleted selected for a retry.
    void selectTracks(const QSet<QString> &paths);

  signals:
    void activated(const Track &track);
    void selected(const Track &track);
    void cursorRestored(const Track &track);
    void changed(const std::shared_ptr<Playlist::Playlist> pl);
    void durationOfSelectedChanged(quint32 total_duration);
    void createPlaylistRequested(const QList<QDir> &filepaths, const QString &libraryDir);
    void createPlaylistFromTracksRequested(const QVector<Track> &tracks, const QString &name);
    void trackInfoRequested(const Track &track);
    void favoriteToggled(const Track &track);
    void deleteFilesRequested(const QVector<Track> &tracks);
    // Non-zero match count only while a search term is active.
    void searchResultsChanged(int matched, int total);

  public slots:
    void on_load(const std::shared_ptr<Playlist::Playlist> pi);
    void on_unload();
    void on_stop();
    void on_start(const Track &t);
    void on_pause(const Track &t);
    void on_scrollTo(const Track &track);
    void on_trackMetaChanged(const Track &t);
    void on_appendToPlaylist(const QList<QDir> &filepaths);
    void on_appendTracks(const QVector<Track> &tracks);
    void on_removeTracks(quint64 playlist_uid, const QVector<Track> &tracks);
    void on_tracksAppended(const std::shared_ptr<Playlist::Playlist> pl);
    void sortBy(const QString &criteria);
    void on_tracksChanged(const std::shared_ptr<Playlist::Playlist> pl, const QList<quint64> &uids);

  private slots:
    void removeSelectedTracks();
    void onSectionResized(int section, int old_size, int new_size);
    void updateSearchStatus();
    void on_appendAsyncFinished(std::shared_ptr<Playlist::Playlist> pl);
    void on_search(const QString &term);
    void on_currentSelectionChanged(const QModelIndex &index, const QModelIndex &prev);
    void on_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

  private:
    QTableView *view;
    QLineEdit *search;
    BackgroundTasks *tasks;
    Config::Local &local_conf;
    Config::Global &global_conf;
    ModusOperandi &modus_operandi;
    bool restore_scroll_once;
    bool persist_pending = false;
    QHash<quint64,int> scroll_positions;
    ProxyFilterModel *proxy;
    PlaylistContextMenu *context_menu;
    ColumnsConfig columns_config;
    quint64 live_stream_uid = 0;
    bool restoring_cursor = false;

    QString dirsLabel(const QList<QDir> &dirs) const;
    void updateStreamSpans();
    void restoreCursor(quint64 cursor_uid, int fallback_row);
    void eventFilterTableView(QEvent *event);
    QTimer *columns_save_timer = nullptr;
    bool applying_widths = false;
    bool eventFilterSearch(QEvent *event);
    void eventFilterViewport(QEvent *event);
    bool handleDnd(QEvent *event);
    void onDrop(QDropEvent *event);
    const TracksMimeData *droppedTracks(QDropEvent *event) const;

    void loadColumnsConfig();

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
  };
}

#endif // PLAYLISTVIEW_H
