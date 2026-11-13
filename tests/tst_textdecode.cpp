#include <QtTest>

#include "textdecode.h"

class TestTextDecode : public QObject {
  Q_OBJECT
private slots:
  void repairsGbkId3v1Mojibake();
  void repairsTrackTitle();
  void repairsMixedLatinAndChinese();
  void repairsTruncatedField();
  void repairsUtf8StoredInLatin1Field();
  void keepsProperUnicode();
  void keepsLatin1Names();
  void keepsAscii();
  void keepsEmpty();
  void keepsGarbageThatIsNotChinese();
};

void TestTextDecode::repairsGbkId3v1Mojibake() {
  // "苏打绿" as GBK bytes read back as ISO-8859-1.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("ËÕ´òÂÌ")), QStringLiteral("苏打绿"));
}

void TestTextDecode::repairsTrackTitle() {
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Ãµ¹å»¨µÄÔáÀñ")), QStringLiteral("玫瑰花的葬礼"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("ÎÞÓëÂ×±ÈµÄÃÀÀö")), QStringLiteral("无与伦比的美丽"));
}

void TestTextDecode::repairsMixedLatinAndChinese() {
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("F.I.R(·É¶ùÀÖÍÅ)")), QStringLiteral("F.I.R(飞儿乐团)"));
  // Trailing spaces come from the fixed-width ID3v1 field.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("ËÕ´òÂÌ   ")), QStringLiteral("苏打绿"));
}

void TestTextDecode::repairsTruncatedField() {
  // "无与伦比的美丽" is 14 GBK bytes; the last character got cut in half.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("ÎÞÓëÂ×±ÈµÄÃÀÀ")), QStringLiteral("无与伦比的美"));
}

void TestTextDecode::repairsUtf8StoredInLatin1Field() {
  // Some tools wrote UTF-8 into the ID3v1 field; it reads back as Latin-1.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("HÃ©lÃ¨ne RollÃ¨s")), QStringLiteral("Hélène Rollès"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("BjÃ¶rk")), QStringLiteral("Björk"));
}

void TestTextDecode::keepsProperUnicode() {
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("玫瑰花的葬礼")), QStringLiteral("玫瑰花的葬礼"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Русский")), QStringLiteral("Русский"));
}

void TestTextDecode::keepsLatin1Names() {
  // Real accented names must not be "repaired" into ideographs.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Édith Piaf")), QStringLiteral("Édith Piaf"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Mötley Crüe")), QStringLiteral("Mötley Crüe"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Björk")), QStringLiteral("Björk"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Café Müller")), QStringLiteral("Café Müller"));
}

void TestTextDecode::keepsAscii() {
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Nevermind")), QStringLiteral("Nevermind"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("01 - Track")), QStringLiteral("01 - Track"));
}

void TestTextDecode::keepsEmpty() {
  QVERIFY(TextDecode::fromLegacyMojibake(QString()).isEmpty());
}

void TestTextDecode::keepsGarbageThatIsNotChinese() {
  // These decode to a single ideograph through GB18030, but they are Latin
  // names, so the repair has to leave them alone.
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Über")), QStringLiteral("Über"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("Ça")), QStringLiteral("Ça"));
  QCOMPARE(TextDecode::fromLegacyMojibake(QStringLiteral("×÷")), QStringLiteral("×÷"));
}

QTEST_APPLESS_MAIN(TestTextDecode)
#include "tst_textdecode.moc"
