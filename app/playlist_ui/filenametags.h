#ifndef FILENAMETAGS_H
#define FILENAMETAGS_H

#include <QString>

namespace PlaylistUi {
  // What a file name tells us about a track. Rippers are wildly inconsistent,
  // so this only offers a guess: the caller shows it for review before writing.
  struct FilenameTags {
    QString artist;
    QString title;
    int track = 0;
    bool valid = false;
  };

  // Understands the shapes that show up in real collections:
  //   "Artist - Title", "Artist-Title", "01. Title", "01 - Title",
  //   "01_Artist - Title", and the "Title - Artist" order with swap=true.
  FilenameTags parseTrackFilename(const QString &fileName, bool title_first = false);
}

#endif // FILENAMETAGS_H
