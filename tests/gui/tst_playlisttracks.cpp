#include "fixture.h"

#include "backgroundtasks.h"
#include "modusoperandi.h"
#include "playlist_ui/playlistcontextmenu.h"
#include "playlist_ui/tageditordialog.h"
#include "playlist_ui/playlistcontroller.h"
#include "slidingbanner.h"

#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QTableView>
#include <QHeaderView>

namespace {
  // The editor is created parentless, so it shows up as a top-level widget.
  TagEditorDialog *openEditor() {
    for (auto *widget : qApp->topLevelWidgets()) {
      if (auto *editor = qobject_cast<TagEditorDialog *>(widget)) {
        return editor;
      }
    }
    return nullptr;
  }

  QMimeData *rowsMime(const QList<int> &rows) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << static_cast<qint32>(rows.size());
    for (int r : rows) {
      stream << static_cast<qint32>(r);
    }
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("application/x-mpz-playlist-tracks"), bytes);
    return mime;
  }
}

class TestPlaylistTracks : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();
  void appendTracksFillsTheView();
  void appendFromDiskScansTheDirectory();
  void rearrangeMovesOneRowAndReportsTheChange();
  void rearrangeMovesAContiguousBlock();
  void deleteKeyRemovesTheSelectionAndRestoresTheCursor();
  void filterMatchesArtistAlbumTitleAndFilename();
  void searchReportsHowManyTracksMatch();
  void enterInSearchJumpsToTheFirstMatch();
  void selectTrackCentersTheGivenPath();
  void favoriteColumnReflectsFavoritePaths();
  void clickingTheHeartColumnEmitsFavoriteToggled();
  void draggingAColumnKeepsTheNewWidth();
  void escapeInSearchClearsTheFilter();
  void sortByReordersThePlaylist();
  void unloadClearsTheView();
  void editTagsSkipsStreamsAndOpensForTheRest();
  void copyNameJoinsTheSelectedTitles();

private:
  GuiTest::ConfigDir config;
  SlidingBanner banner;
  QTableView view;
  QLineEdit search;
  std::unique_ptr<Config::Global> global;
  std::unique_ptr<Config::Local> local;
  std::unique_ptr<ModusOperandi> modus;
  std::unique_ptr<BackgroundTasks> tasks;
  std::unique_ptr<PlaylistUi::Controller> controller;
  std::shared_ptr<Playlist::Playlist> playlist;

  void load(const QStringList &titles);
  QStringList shownTitles() const;
};

void TestPlaylistTracks::initTestCase() {
  QVERIFY(config.init());
}

void TestPlaylistTracks::init() {
  // The search field outlives the controller, so a term from the previous test
  // would survive into the next one.
  search.clear();
  global = std::make_unique<Config::Global>();
  local = std::make_unique<Config::Local>();
  modus = std::make_unique<ModusOperandi>(*local, &banner);
  tasks = std::make_unique<BackgroundTasks>();
  controller = std::make_unique<PlaylistUi::Controller>(&view, &search, tasks.get(), *local, *global, *modus);
  playlist = std::make_shared<Playlist::Playlist>();
  playlist->rename("test");
}

void TestPlaylistTracks::cleanup() {
  controller.reset();
  tasks.reset();
  modus.reset();
  local.reset();
  global.reset();
}

void TestPlaylistTracks::load(const QStringList &titles) {
  playlist->load(GuiTest::tracks(titles));
  controller->on_load(playlist);
}

QStringList TestPlaylistTracks::shownTitles() const {
  QStringList result;
  for (int i = 0; i < view.model()->rowCount(); i++) {
    // Column 0 is the play-state gutter; title lives in the configured columns.
    result << view.model()->index(i, 3).data().toString();
  }
  return result;
}

void TestPlaylistTracks::appendTracksFillsTheView() {
  controller->on_load(playlist);
  QSignalSpy changed(controller.get(), &PlaylistUi::Controller::changed);

  controller->on_appendTracks(GuiTest::tracks({"one", "two"}));

  QCOMPARE(view.model()->rowCount(), 2);
  QCOMPARE(shownTitles(), QStringList({"one", "two"}));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(controller->currentTracks().size(), 2);
}

void TestPlaylistTracks::appendFromDiskScansTheDirectory() {
  const QString music = config.path() + "/append";
  QVERIFY(GuiTest::copyAudioFixtures(music));
  controller->on_load(playlist);
  QSignalSpy changed(controller.get(), &PlaylistUi::Controller::changed);

  controller->on_appendToPlaylist({QDir(music)});

  QVERIFY(changed.wait());
  QCOMPARE(view.model()->rowCount(), 3);
}

void TestPlaylistTracks::rearrangeMovesOneRowAndReportsTheChange() {
  load({"one", "two", "three"});
  QSignalSpy changed(controller.get(), &PlaylistUi::Controller::changed);
  std::unique_ptr<QMimeData> mime(rowsMime({0}));

  view.model()->dropMimeData(mime.get(), Qt::MoveAction, 3, 0, QModelIndex());

  QCOMPARE(shownTitles(), QStringList({"two", "three", "one"}));
  QCOMPARE(playlist->tracks().first().title(), QString("two"));
  // The controller coalesces rowsMoved into one changed() on the next event turn.
  QCOMPARE(changed.count(), 0);
  QVERIFY(changed.wait());
  QCOMPARE(changed.count(), 1);
}

void TestPlaylistTracks::rearrangeMovesAContiguousBlock() {
  load({"one", "two", "three", "four"});
  std::unique_ptr<QMimeData> mime(rowsMime({2, 3}));

  view.model()->dropMimeData(mime.get(), Qt::MoveAction, 0, 0, QModelIndex());

  QCOMPARE(shownTitles(), QStringList({"three", "four", "one", "two"}));
}

void TestPlaylistTracks::deleteKeyRemovesTheSelectionAndRestoresTheCursor() {
  load({"one", "two", "three"});
  view.selectRow(0);
  QSignalSpy changed(controller.get(), &PlaylistUi::Controller::changed);

  QTest::keyClick(&view, Qt::Key_Delete);

  QCOMPARE(shownTitles(), QStringList({"two", "three"}));
  QCOMPARE(playlist->tracks().size(), 2);
  QCOMPARE(changed.count(), 1);
  QCOMPARE(view.currentIndex().row(), 0);
}

void TestPlaylistTracks::filterMatchesArtistAlbumTitleAndFilename() {
  playlist->load({GuiTest::track("Dogs", "Animals", "Pink Floyd"),
                  GuiTest::track("Schism", "Lateralus", "Tool")});
  controller->on_load(playlist);

  search.setText("Tool");
  QCOMPARE(shownTitles(), QStringList({"Schism"}));

  search.setText("animals");
  QCOMPARE(shownTitles(), QStringList({"Dogs"}));

  search.setText("Dogs.mp3");
  QCOMPARE(shownTitles(), QStringList({"Dogs"}));

  search.clear();
  QCOMPARE(view.model()->rowCount(), 2);
}

void TestPlaylistTracks::searchReportsHowManyTracksMatch() {
  playlist->load({GuiTest::track("Dogs", "Animals", "Pink Floyd"),
                  GuiTest::track("Schism", "Lateralus", "Tool"),
                  GuiTest::track("Toolbox", "Toolbox", "Tool")});
  controller->on_load(playlist);
  QSignalSpy spy(controller.get(), &PlaylistUi::Controller::searchResultsChanged);

  search.setText("Tool");

  QVERIFY(!spy.isEmpty());
  QCOMPARE(spy.last().at(0).toInt(), 2);
  QCOMPARE(spy.last().at(1).toInt(), 3);

  // Rows appearing while a term is active update the count too.
  controller->on_appendTracks({GuiTest::track("Toolbox 2", "Toolbox", "Tool")});
  QCOMPARE(spy.last().at(0).toInt(), 3);
  QCOMPARE(spy.last().at(1).toInt(), 4);
}

void TestPlaylistTracks::enterInSearchJumpsToTheFirstMatch() {
  load({"aaa", "bbb", "ccc"});
  search.setText("bbb");
  QCOMPARE(view.model()->rowCount(), 1);

  QTest::keyClick(&search, Qt::Key_Return);

  QCOMPARE(view.currentIndex().row(), 0);
  QVERIFY(view.selectionModel()->isRowSelected(0, QModelIndex()));
  // The term is kept, so pressing Enter again plays the highlight.
  QCOMPARE(search.text(), QStringLiteral("bbb"));
}

void TestPlaylistTracks::favoriteColumnReflectsFavoritePaths() {
  load({"one", "two"});
  QAbstractItemModel *model = view.model();
  const int heart_column = model->columnCount() - 1;

  QCOMPARE(model->index(0, heart_column).data(PlaylistUi::Model::FavoriteRole).toBool(), false);

  controller->setFavoritePaths({"/music/two.mp3"});

  QCOMPARE(model->index(0, heart_column).data(PlaylistUi::Model::FavoriteRole).toBool(), false);
  QCOMPARE(model->index(1, heart_column).data(PlaylistUi::Model::FavoriteRole).toBool(), true);
  // The heart must not move the configured columns around.
  QCOMPARE(model->index(1, 3).data().toString(), QString("two"));
}

void TestPlaylistTracks::clickingTheHeartColumnEmitsFavoriteToggled() {
  load({"one", "two"});
  QSignalSpy spy(controller.get(), &PlaylistUi::Controller::favoriteToggled);
  const int heart_column = view.model()->columnCount() - 1;
  const QRect cell = view.visualRect(view.model()->index(1, heart_column));

  QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, cell.center());

  QCOMPARE(spy.count(), 1);
  QCOMPARE(spy.first().first().value<Track>().path(), QString("/music/two.mp3"));
}

void TestPlaylistTracks::draggingAColumnKeepsTheNewWidth() {
  view.resize(900, 400);
  view.show();
  QVERIFY(QTest::qWaitForWindowExposed(&view));
  load({"one"});
  QTest::qWait(50);

  const int before = view.columnWidth(1);
  view.horizontalHeader()->resizeSection(1, 120);
  QTest::qWait(600); // the config write is debounced

  QCOMPARE(view.columnWidth(1), 120);
  QVERIFY(view.columnWidth(1) != before);

  const auto widths = global->columnsConfig();
  QVERIFY(widths.width(1) < 0.28);
  QVERIFY(widths.width(1) > 0.0);
}

void TestPlaylistTracks::selectTrackCentersTheGivenPath() {
  load({"one", "two", "three"});

  controller->selectTrack("/music/two.mp3");

  QCOMPARE(view.currentIndex().row(), 1);
  QVERIFY(view.selectionModel()->isRowSelected(1, QModelIndex()));

  // A track the active filter hides must not steal the cursor from the view.
  search.setText("three");
  const QModelIndex before = view.currentIndex();
  controller->selectTrack("/music/one.mp3");
  QCOMPARE(view.currentIndex(), before);
}

void TestPlaylistTracks::escapeInSearchClearsTheFilter() {
  load({"aaa", "bbb"});
  search.setText("bbb");
  QCOMPARE(view.model()->rowCount(), 1);

  QTest::keyClick(&search, Qt::Key_Escape);

  QVERIFY(search.text().isEmpty());
  QCOMPARE(view.model()->rowCount(), 2);
}

void TestPlaylistTracks::sortByReordersThePlaylist() {
  load({"zzz", "aaa", "mmm"});

  controller->sortBy("Title");
  QCOMPARE(shownTitles(), QStringList({"aaa", "mmm", "zzz"}));

  controller->sortBy("-Title");
  QCOMPARE(shownTitles(), QStringList({"zzz", "mmm", "aaa"}));
}

void TestPlaylistTracks::unloadClearsTheView() {
  load({"one", "two"});

  controller->on_unload();

  QCOMPARE(view.model()->rowCount(), 0);
  QVERIFY(controller->currentTracks().isEmpty());
}

void TestPlaylistTracks::editTagsSkipsStreamsAndOpensForTheRest() {
  playlist->load({GuiTest::track("one"),
                  Track(QUrl("http://radio.example/live"), "radio://x", "Station"),
                  GuiTest::track("two")});
  controller->on_load(playlist);
  auto *menu = controller->findChild<PlaylistUi::PlaylistContextMenu *>();
  QVERIFY(menu != nullptr);

  // A stream alone has nothing to edit, so no editor opens.
  view.selectRow(1);
  QVERIFY(QMetaObject::invokeMethod(menu, "on_editTags"));
  QVERIFY(openEditor() == nullptr);

  view.selectRow(0);
  view.selectionModel()->select(view.model()->index(1, 0),
                                QItemSelectionModel::Select | QItemSelectionModel::Rows);
  view.selectionModel()->select(view.model()->index(2, 0),
                                QItemSelectionModel::Select | QItemSelectionModel::Rows);
  QVERIFY(QMetaObject::invokeMethod(menu, "on_editTags"));

  auto *editor = openEditor();
  QVERIFY(editor != nullptr);
  // The stream was dropped from the three selected rows.
  QCOMPARE(editor->findChild<QLabel *>(QStringLiteral("labelHeader"))->text(),
           QString("Editing 2 track(s)"));
  editor->close();
}

void TestPlaylistTracks::copyNameJoinsTheSelectedTitles() {
  load({"one", "two", "three"});
  auto *menu = controller->findChild<PlaylistUi::PlaylistContextMenu *>();
  qApp->clipboard()->clear();

  view.selectRow(0);
  view.selectionModel()->select(view.model()->index(2, 0),
                                QItemSelectionModel::Select | QItemSelectionModel::Rows);
  QVERIFY(QMetaObject::invokeMethod(menu, "on_copyName"));

  const QStringList lines = qApp->clipboard()->text().split('\n');
  QCOMPARE(lines.size(), 2);
  QVERIFY(lines.first().contains("one"));
  QVERIFY(lines.last().contains("three"));
}

MPZ_GUI_TEST_MAIN(TestPlaylistTracks)
#include "tst_playlisttracks.moc"
