#ifndef TAGSFROMFILENAMEDIALOG_H
#define TAGSFROMFILENAMEDIALOG_H

#include "track.h"

#include <QDialog>
#include <QVector>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace PlaylistUi {
  // Offers artist/title/track numbers parsed from file names. Nothing is
  // written until the table has been reviewed and applied.
  class TagsFromFilenameDialog : public QDialog {
    Q_OBJECT
  public:
    explicit TagsFromFilenameDialog(const QVector<Track> &tracks, QWidget *parent = nullptr);

  signals:
    void saved(const QList<quint64> &uids);

  private slots:
    void rebuild();
    void apply();

  private:
    struct Entry {
      Track track;
      QString filename;
      QString existing_artist;
      QString existing_title;
    };

    QVector<Entry> entries;
    QTableWidget *table = nullptr;
    QCheckBox *overwrite = nullptr;
    QCheckBox *title_first = nullptr;
    QLabel *status = nullptr;
    QPushButton *apply_button = nullptr;
  };
}

#endif // TAGSFROMFILENAMEDIALOG_H
