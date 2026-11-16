#include "fixture.h"

#include "config/global.h"
#include "config/local.h"
#include "ipc/instance.h"
#include "mainwindow.h"
#include "playback/playbackcontroller.h"
#include "playlists_ui/playlistscontroller.h"
#include "playlists_ui/playlistsmodel.h"
#include "playlists_ui/playlistsproxyfiltermodel.h"
#include "shortcuts.h"
#include "trayicon.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTableView>
#include <QToolButton>
#include <QTreeView>

namespace {
  QPoint checkboxIndicator(QCheckBox *box) {
    QStyleOptionButton opt;
    opt.initFrom(box);
    return box->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &opt, box).center();
  }
}

// One window for the whole binary: Config::Global, Config::Local and ModusOperandi are single-instance guarded, and CoverArt::Covers is a
// process-wide singleton that captures the first window's ModusOperandi. The window is never close()d either -- closeEvent ends in qApp->quit().
class TestMainWindow : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void buildsWithControlsAndViews();
  void followCursorClickWritesConfig();
  void orderComboBoxKeyboardWritesConfig();
  void perPlaylistOrderFollowsTheSelectedPlaylist();
  void focusShortcutsReachTheThreeViews();
  void volumeShortcutsStepByFiveAndClamp();
  void controlsToolbarIsNamedAndLockable();
  void dockWidgetsExistAndStartHidden();
  void windowTitleFollowsPlayback();
  void windowTitleIsElidedForLongTracks();
  void streamBufferDefaultIsPersisted();
  void trayIconStaysOffWhenDisabled();
  void crossPlaylistSearchListsHitsAndJumpsToThem();
  void heartClickAddsTheTrackToFavorites();
  void deleteFilesDialogRemovesTheFileFromDisk();

private:
  GuiTest::ConfigDir config;
  std::unique_ptr<Config::Global> global;
  std::unique_ptr<Config::Local> local;
  std::unique_ptr<IPC::Instance> instance;
  std::unique_ptr<MainWindow> window;
};

void TestMainWindow::initTestCase() {
  QVERIFY(config.init({config.path() + "/library"}));
  QVERIFY(QDir().mkpath(config.path() + "/library"));

  global = std::make_unique<Config::Global>();
  local = std::make_unique<Config::Local>();
  instance = std::make_unique<IPC::Instance>();
  window = std::make_unique<MainWindow>(QStringList(), instance.get(), *local, *global);
  window->show();
  QVERIFY(QTest::qWaitForWindowExposed(window.get()));
}

void TestMainWindow::cleanupTestCase() {
  window.reset();
  instance.reset();
  local.reset();
  global.reset();
}

void TestMainWindow::buildsWithControlsAndViews() {
  for (const char *name : {"playButton", "pauseButton", "stopButton", "prevButton", "nextButton"}) {
    auto *button = window->findChild<QToolButton *>(QString::fromLatin1(name));
    QVERIFY2(button != nullptr, name);
    QVERIFY2(button->isVisible(), name);
  }

  QVERIFY(window->findChild<QTreeView *>(QStringLiteral("treeView")) != nullptr);
  QVERIFY(window->findChild<QListView *>(QStringLiteral("listView")) != nullptr);
  QVERIFY(window->findChild<QTableView *>(QStringLiteral("tableView")) != nullptr);
  QVERIFY(window->findChild<QComboBox *>(QStringLiteral("comboBoxLibraries")) != nullptr);
  QVERIFY(window->findChild<QLineEdit *>(QStringLiteral("treeViewSearch")) != nullptr);
  QVERIFY(window->statusBar() != nullptr);
}

void TestMainWindow::followCursorClickWritesConfig() {
  auto *box = window->findChild<QCheckBox *>(QStringLiteral("followCursorCheckBox"));
  QVERIFY(box != nullptr);
  const bool expected = !box->isChecked();

  QTest::mouseClick(box, Qt::LeftButton, Qt::NoModifier, checkboxIndicator(box));

  QCOMPARE(box->isChecked(), expected);
  QCOMPARE(global->playbackFollowCursor(), expected);
}

void TestMainWindow::orderComboBoxKeyboardWritesConfig() {
  auto *combo = window->findChild<QComboBox *>(QStringLiteral("orderComboBox"));
  QVERIFY(combo != nullptr);
  combo->setCurrentIndex(0);

  QTest::keyClick(combo, Qt::Key_Down);

  QCOMPARE(combo->currentIndex(), 1);
  QCOMPARE(global->playbackOrder(), QString("random"));
}

void TestMainWindow::perPlaylistOrderFollowsTheSelectedPlaylist() {
  auto *combo = window->findChild<QComboBox *>(QStringLiteral("perPlaylistOrdercomboBox"));
  QVERIFY(combo != nullptr);
  QCOMPARE(combo->count(), 4);
  QCOMPARE(combo->itemText(0), QString("(use global)"));

  auto *playlists = window->findChild<PlaylistsUi::Controller *>();
  QVERIFY(playlists != nullptr);
  QSignalSpy created(playlists, &PlaylistsUi::Controller::loaded);
  playlists->on_createPlaylistFromTracks(GuiTest::tracks({"one", "two"}), "ordered");
  QVERIFY(created.wait());

  auto playlist = playlists->currentPlaylist();
  QVERIFY(playlist != nullptr);
  playlist->setRandom(Playlist::Playlist::SequentialNoLoop);
  playlists->on_jumpTo(playlist);
  QCOMPARE(combo->currentIndex(), 3);
}

void TestMainWindow::focusShortcutsReachTheThreeViews() {
  auto *shortcuts = window->findChild<Shortcuts *>();
  QVERIFY(shortcuts != nullptr);

  emit shortcuts->focusLibrary();
  QVERIFY(window->findChild<QTreeView *>(QStringLiteral("treeView"))->hasFocus());

  emit shortcuts->focusPlaylists();
  QVERIFY(window->findChild<QListView *>(QStringLiteral("listView"))->hasFocus());

  emit shortcuts->focusPlaylist();
  QVERIFY(window->findChild<QTableView *>(QStringLiteral("tableView"))->hasFocus());

  emit shortcuts->focusFilterLibrary();
  QVERIFY(window->findChild<QLineEdit *>(QStringLiteral("treeViewSearch"))->hasFocus());
}

void TestMainWindow::volumeShortcutsStepByFiveAndClamp() {
  auto *shortcuts = window->findChild<Shortcuts *>();
  auto *player = window->findChild<Playback::Controller *>();
  QVERIFY(player != nullptr);

  player->setVolume(50);
  emit shortcuts->volumeUp();
  QCOMPARE(player->volume(), 55);

  emit shortcuts->volumeDown();
  QCOMPARE(player->volume(), 50);

  player->setVolume(98);
  emit shortcuts->volumeUp();
  QCOMPARE(player->volume(), 100);

  player->setVolume(3);
  emit shortcuts->volumeDown();
  QCOMPARE(player->volume(), 0);
}

void TestMainWindow::controlsToolbarIsNamedAndLockable() {
  // The object name is what makes saveState()/restoreState() find it again.
  auto *toolbar = window->findChild<QToolBar *>(QStringLiteral("controlsToolBar"));
  QVERIFY(toolbar != nullptr);
  QCOMPARE(toolbar->isMovable(), local->toolbarMovable());

  QAction *lock = nullptr;
  for (auto *action : window->findChildren<QAction *>()) {
    if (action->text() == "Lock toolbar") {
      lock = action;
    }
  }
  QVERIFY(lock != nullptr);
  QVERIFY(lock->isCheckable());

  lock->setChecked(false);
  QVERIFY(toolbar->isMovable());
  QVERIFY(local->toolbarMovable());

  lock->setChecked(true);
  QVERIFY(!toolbar->isMovable());
  QVERIFY(!local->toolbarMovable());
}

void TestMainWindow::dockWidgetsExistAndStartHidden() {
  auto *cover = window->findChild<QDockWidget *>(QStringLiteral("coverArtDock"));
  auto *lyrics = window->findChild<QDockWidget *>(QStringLiteral("lyricsDock"));
  QVERIFY(cover != nullptr && lyrics != nullptr);

  // Hidden on a first run; restoreState() brings them back afterwards.
  QVERIFY(cover->isHidden());
  QVERIFY(lyrics->isHidden());
  QVERIFY(!cover->toggleViewAction()->isChecked());
}

void TestMainWindow::windowTitleFollowsPlayback() {
  auto *player = window->findChild<Playback::Controller *>();
  const QString idle = qApp->applicationDisplayName();
  QCOMPARE(window->windowTitle(), idle);

  const Track track = GuiTest::track("song");
  emit player->started(track);
  QCOMPARE(window->windowTitle(), "[" + track.shortText() + "] " + idle);

  emit player->stopped();
  QCOMPARE(window->windowTitle(), idle);
}

void TestMainWindow::windowTitleIsElidedForLongTracks() {
  auto *player = window->findChild<Playback::Controller *>();
  const int status_bar_width = window->statusBar()->minimumSizeHint().width();

  emit player->started(GuiTest::track(QString("title").repeated(40), "album", QString("artist").repeated(40)));

  QVERIFY(window->windowTitle().size() < 128);
  QVERIFY(window->windowTitle().endsWith(QString(QChar(0x2026)) + "] " + qApp->applicationDisplayName()));
  QCOMPARE(window->statusBar()->minimumSizeHint().width(), status_bar_width);
}

void TestMainWindow::streamBufferDefaultIsPersisted() {
  QCOMPARE(global->streamBufferSize(), 262144);
}

void TestMainWindow::crossPlaylistSearchListsHitsAndJumpsToThem() {
  auto *playlists_view = window->findChild<QListView *>("listView");
  auto *proxy = playlists_view == nullptr ? nullptr : qobject_cast<PlaylistsUi::ProxyFilterModel *>(playlists_view->model());
  auto *model = proxy == nullptr ? nullptr : qobject_cast<PlaylistsUi::Model *>(proxy->sourceModel());
  QVERIFY(playlists_view != nullptr);
  QVERIFY(proxy != nullptr);
  QVERIFY(model != nullptr);

  auto playlistNamed = [model](const QString &name) -> std::shared_ptr<Playlist::Playlist> {
    for (const auto &p : model->itemList()) {
      if (p->name() == name) {
        return p;
      }
    }
    return nullptr;
  };

  model->createPlaylistFromTracks(GuiTest::tracks({"Dogs", "Schism"}), "rock");
  QTRY_VERIFY(playlistNamed("rock") != nullptr);

  auto *field = window->findChild<QLineEdit *>("tableViewSearch");
  auto *all_playlists = window->findChild<QCheckBox *>("searchAllPlaylists");
  auto *results = window->findChild<QListWidget *>("crossSearchResults");
  QVERIFY(field != nullptr);
  QVERIFY(all_playlists != nullptr);
  QVERIFY(results != nullptr);
  QVERIFY(!results->isVisible());

  all_playlists->setChecked(true);
  field->setText("Dogs");

  QTRY_VERIFY(results->isVisible());
  QCOMPARE(results->count(), 1);
  QCOMPARE(results->item(0)->data(Qt::UserRole + 1).toString(), QString("/music/Dogs.mp3"));

  // Unchecking puts the popup away again.
  all_playlists->setChecked(false);
  QVERIFY(!results->isVisible());

  // Clicking a hit opens its playlist and leaves the track selected there.
  all_playlists->setChecked(true);
  QTRY_VERIFY(results->isVisible());
  const QRect rect = results->visualItemRect(results->item(0));
  QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier, rect.center());

  QTRY_VERIFY(playlists_view->currentIndex().isValid());
  const auto opened = model->itemAt(proxy->mapToSource(playlists_view->currentIndex()));
  QVERIFY(opened != nullptr);
  QCOMPARE(opened->name(), QString("rock"));
  QVERIFY(!results->isVisible());

  // and the track itself is selected in the track list
  auto *tracks_view = window->findChild<QTableView *>("tableView");
  QVERIFY(tracks_view != nullptr);
  QTRY_VERIFY(tracks_view->currentIndex().isValid());
  QCOMPARE(tracks_view->currentIndex().siblingAtColumn(3).data().toString(), QString("Dogs"));
}

void TestMainWindow::heartClickAddsTheTrackToFavorites() {
  auto *playlists_view = window->findChild<QListView *>("listView");
  auto *proxy = qobject_cast<PlaylistsUi::ProxyFilterModel *>(playlists_view->model());
  auto *model = qobject_cast<PlaylistsUi::Model *>(proxy->sourceModel());
  QVERIFY(model != nullptr);

  int rock_row = -1;
  for (int i = 0; i < model->rowCount(); ++i) {
    const auto pl = model->itemAt(model->index(i, 0));
    if (pl != nullptr && pl->name() == "rock") {
      rock_row = i;
      break;
    }
  }
  QVERIFY(rock_row >= 0);
  const QModelIndex rock = proxy->index(rock_row, 0);
  QTest::mouseClick(playlists_view->viewport(), Qt::LeftButton, Qt::NoModifier, playlists_view->visualRect(rock).center());

  auto *tracks_view = window->findChild<QTableView *>("tableView");
  QVERIFY(tracks_view != nullptr);
  QTRY_VERIFY(tracks_view->model()->rowCount() > 0);

  const int heart_column = tracks_view->model()->columnCount() - 1;
  const QRect cell = tracks_view->visualRect(tracks_view->model()->index(0, heart_column));
  QTest::mouseClick(tracks_view->viewport(), Qt::LeftButton, Qt::NoModifier, cell.center());

  std::shared_ptr<Playlist::Playlist> favorites;
  QTRY_VERIFY([&]() {
    for (const auto &p : model->itemList()) {
      if (p->favorites()) {
        favorites = p;
        return true;
      }
    }
    return false;
  }());
  QCOMPARE(favorites->name(), QString("Favorites"));
  QCOMPARE(favorites->tracks().size(), 1);
}

void TestMainWindow::deleteFilesDialogRemovesTheFileFromDisk() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.path() + "/kill-me.mp3";
  {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not really an mp3");
  }
  const Track track(path, 0, "artist", "album", "kill me", 1, 2000, 1000, 2, 320, 44100);

  // The handler is modal by design; answer the dialog from inside its event loop.
  QTimer::singleShot(100, []() {
    for (QWidget *w : QApplication::topLevelWidgets()) {
      if (auto *box = qobject_cast<QMessageBox *>(w)) {
        for (auto *button : box->buttons()) {
          if (button->text().contains("Delete Permanently")) {
            button->click();
            return;
          }
        }
      }
    }
  });

  window->onDeleteFilesRequested({track});

  QVERIFY(!QFile::exists(path));
  QVERIFY(!QDir().exists(path));
}

void TestMainWindow::trayIconStaysOffWhenDisabled() {
  QVERIFY(!global->trayIconEnabled());
  QVERIFY(window->findChild<TrayIcon *>() == nullptr);
}

MPZ_GUI_TEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
