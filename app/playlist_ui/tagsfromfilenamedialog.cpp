#include "tagsfromfilenamedialog.h"
#include "filenametags.h"

#include <fileref.h>
#include <tag.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
  TagLib::String to_taglib(const QString &s) {
    const QByteArray utf8 = s.toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
  }

  void read_existing(const QString &path, QString *artist, QString *title) {
    TagLib::FileRef f(QFile::encodeName(path).constData(), false);
    if (f.isNull() || !f.tag()) {
      return;
    }
    *artist = QString::fromUtf8(f.tag()->artist().toCString(true));
    *title = QString::fromUtf8(f.tag()->title().toCString(true));
  }
}

namespace PlaylistUi {
  TagsFromFilenameDialog::TagsFromFilenameDialog(const QVector<Track> &tracks, QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Fill tags from file name"));
    resize(900, 520);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(tr("Review the values below, uncheck what should not be written, then apply. "
                               "Columns for artist and title can be edited in place."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *options = new QHBoxLayout;
    overwrite = new QCheckBox(tr("Overwrite existing tags"), this);
    title_first = new QCheckBox(tr("File name is \"Title - Artist\""), this);
    options->addWidget(overwrite);
    options->addWidget(title_first);
    options->addStretch();
    layout->addLayout(options);

    table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({tr("Apply"), tr("File"), tr("Artist"), tr("Title")});
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    layout->addWidget(table, 1);

    status = new QLabel(this);
    layout->addWidget(status);

    auto *buttons = new QDialogButtonBox(this);
    apply_button = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &TagsFromFilenameDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &TagsFromFilenameDialog::reject);

    entries.reserve(tracks.size());
    for (const Track &t : tracks) {
      Entry e;
      e.track = t;
      e.filename = QFileInfo(t.path()).fileName();
      read_existing(t.path(), &e.existing_artist, &e.existing_title);
      entries << e;
    }

    connect(overwrite, &QCheckBox::toggled, this, &TagsFromFilenameDialog::rebuild);
    connect(title_first, &QCheckBox::toggled, this, &TagsFromFilenameDialog::rebuild);

    rebuild();
  }

  void TagsFromFilenameDialog::rebuild() {
    table->setRowCount(0);
    table->setRowCount(entries.size());

    const bool replace = overwrite->isChecked();
    for (int row = 0; row < entries.size(); ++row) {
      const Entry &e = entries.at(row);
      FilenameTags guess = parseTrackFilename(e.filename, title_first->isChecked());

      const QString artist = (replace || e.existing_artist.isEmpty()) ? guess.artist : e.existing_artist;
      QString title = (replace || e.existing_title.isEmpty()) ? guess.title : e.existing_title;
      if (title.isEmpty()) {
        title = QFileInfo(e.filename).completeBaseName();
      }

      auto *check = new QTableWidgetItem;
      check->setCheckState(Qt::Checked);
      check->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      table->setItem(row, 0, check);

      auto *file = new QTableWidgetItem(e.filename);
      file->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      file->setToolTip(e.track.path());
      table->setItem(row, 1, file);

      table->setItem(row, 2, new QTableWidgetItem(artist));
      table->setItem(row, 3, new QTableWidgetItem(title));
    }

    table->resizeColumnsToContents();
    table->setColumnWidth(1, qMin(table->columnWidth(1), 260));
    table->setColumnWidth(2, qMin(table->columnWidth(2), 200));
    status->setText(tr("%n track(s) to review", "", entries.size()));
  }

  void TagsFromFilenameDialog::apply() {
    int written = 0;
    QStringList failed;
    QList<quint64> uids;

    for (int row = 0; row < entries.size(); ++row) {
      const auto *check = table->item(row, 0);
      if (check == nullptr || check->checkState() != Qt::Checked) {
        continue;
      }
      const Entry &e = entries.at(row);
      const QString artist = table->item(row, 2) != nullptr ? table->item(row, 2)->text().trimmed() : QString();
      const QString title = table->item(row, 3) != nullptr ? table->item(row, 3)->text().trimmed() : QString();
      if (artist.isEmpty() && title.isEmpty()) {
        continue;
      }

      TagLib::FileRef f(QFile::encodeName(e.track.path()).constData(), false);
      if (f.isNull() || !f.tag()) {
        failed << e.filename;
        continue;
      }
      TagLib::Tag *tag = f.tag();
      if (!artist.isEmpty()) {
        tag->setArtist(to_taglib(artist));
      }
      if (!title.isEmpty()) {
        tag->setTitle(to_taglib(title));
      }
      if (e.track.isCue() == false && e.track.track_number() == 0) {
        const FilenameTags guess = parseTrackFilename(e.filename, title_first->isChecked());
        if (guess.track > 0) {
          tag->setTrack(static_cast<unsigned int>(guess.track));
        }
      }
      if (!f.save()) {
        failed << e.filename;
        continue;
      }
      ++written;
      uids << e.track.uid();
    }

    if (!failed.isEmpty()) {
      status->setText(tr("Written %1, failed %2: %3")
                          .arg(written)
                          .arg(failed.size())
                          .arg(failed.mid(0, 3).join(QStringLiteral(", "))));
      return;
    }
    if (written == 0) {
      status->setText(tr("Nothing selected"));
      return;
    }
    emit saved(uids);
    accept();
  }
}
