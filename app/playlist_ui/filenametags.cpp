#include "filenametags.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace PlaylistUi {
  namespace {
    // "01. ", "1 - ", "01_", "01 "
    const QRegularExpression &numberPrefix() {
      static const QRegularExpression re(QStringLiteral("^\\s*(\\d{1,3})\\s*[.．、_\\-–—－]\\s*(\\S.*)$"));
      return re;
    }

    // "Artist - Title", "Artist – Title", "Artist—Title", "Artist－Title"
    const QRegularExpression &spacedDash() {
      static const QRegularExpression re(QStringLiteral("\\s+[-–—－]\\s+"));
      return re;
    }
  }

  FilenameTags parseTrackFilename(const QString &fileName, bool title_first) {
    FilenameTags result;

    QString base = QFileInfo(fileName).completeBaseName().trimmed();
    if (base.isEmpty()) {
      return result;
    }

    // A year or a bitrate in the name is not a track number: only accept a
    // prefix that is followed by a separator.
    const auto number = numberPrefix().match(base);
    if (number.hasMatch()) {
      result.track = number.captured(1).toInt();
      base = number.captured(2).trimmed();
    }

    QString left;
    QString right;
    const QStringList parts = base.split(spacedDash(), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      left = parts.first().trimmed();
      right = parts.mid(1).join(QStringLiteral(" - ")).trimmed();
    } else {
      // "Artist-Title" without spaces. Only when the left side is short enough
      // to be a name, so "Love-Part 2" style titles are not mangled.
      const int dash = base.indexOf(QLatin1Char('-'));
      if (dash > 0 && dash <= 40 && dash < base.size() - 1) {
        const QString candidate_left = base.left(dash).trimmed();
        const QString candidate_right = base.mid(dash + 1).trimmed();
        if (!candidate_left.isEmpty() && !candidate_right.isEmpty()) {
          left = candidate_left;
          right = candidate_right;
        }
      }
    }

    if (left.isEmpty() || right.isEmpty()) {
      result.title = base;
    } else if (title_first) {
      result.title = left;
      result.artist = right;
    } else {
      result.artist = left;
      result.title = right;
    }
    result.valid = !result.title.isEmpty();
    return result;
  }
}
