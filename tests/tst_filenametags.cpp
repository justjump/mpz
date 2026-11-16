#include <QtTest>

#include "playlist_ui/filenametags.h"

class TestFilenameTags : public QObject {
  Q_OBJECT
private slots:
  void parsesArtistAndTitle();
  void parsesTrackNumbers();
  void parsesDashesWithoutSpaces();
  void swapsTitleFirstOrder();
  void keepsTitlesWithoutArtist();
  void rejectsEmpty();

private:
  static PlaylistUi::FilenameTags parse(const QString &name, bool title_first = false) {
    return PlaylistUi::parseTrackFilename(name, title_first);
  }
};

void TestFilenameTags::parsesArtistAndTitle() {
  const auto a = parse("苏打绿 - 无与伦比的美丽.mp3");
  QCOMPARE(a.artist, QString("苏打绿"));
  QCOMPARE(a.title, QString("无与伦比的美丽"));
  QVERIFY(a.valid);

  const auto b = parse("The Power of Love - Céline Dion.mp3");
  QCOMPARE(b.artist, QString("The Power of Love"));
  QCOMPARE(b.title, QString("Céline Dion"));

  // A title with its own dash keeps the rest together.
  const auto c = parse("周杰伦 - 爱我别走 (Live) - 2001.mp3");
  QCOMPARE(c.artist, QString("周杰伦"));
  QCOMPARE(c.title, QString("爱我别走 (Live) - 2001"));
}

void TestFilenameTags::parsesTrackNumbers() {
  const auto a = parse("08.在北京的金山上.mp3");
  QCOMPARE(a.track, 8);
  QCOMPARE(a.title, QString("在北京的金山上"));
  QVERIFY(a.artist.isEmpty());

  const auto b = parse("03 - 乡恋.mp3");
  QCOMPARE(b.track, 3);
  QCOMPARE(b.title, QString("乡恋"));

  const auto c = parse("12 - 戴玉强 - 颂歌一曲唱韶山.mp3");
  QCOMPARE(c.track, 12);
  QCOMPARE(c.artist, QString("戴玉强"));
  QCOMPARE(c.title, QString("颂歌一曲唱韶山"));
}

void TestFilenameTags::parsesDashesWithoutSpaces() {
  const auto a = parse("胡杨林-香水有毒.mp3");
  QCOMPARE(a.artist, QString("胡杨林"));
  QCOMPARE(a.title, QString("香水有毒"));
}

void TestFilenameTags::swapsTitleFirstOrder() {
  const auto a = parse("Because You Loved Me - Céline Dion.mp3", true);
  QCOMPARE(a.title, QString("Because You Loved Me"));
  QCOMPARE(a.artist, QString("Céline Dion"));
}

void TestFilenameTags::keepsTitlesWithoutArtist() {
  const auto a = parse("精美 目录.mp4");
  QVERIFY(a.artist.isEmpty());
  QCOMPARE(a.title, QString("精美 目录"));

  const auto b = parse("香水有毒.mp3");
  QVERIFY(b.artist.isEmpty());
  QCOMPARE(b.title, QString("香水有毒"));
}

void TestFilenameTags::rejectsEmpty() {
  QVERIFY(!parse("").valid);
  QVERIFY(!parse(".mp3").valid);
}

QTEST_APPLESS_MAIN(TestFilenameTags)
#include "tst_filenametags.moc"
