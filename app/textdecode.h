#ifndef TEXTDECODE_H
#define TEXTDECODE_H

#include <QString>

namespace TextDecode {
  // ID3v1 fields (and some old ID3v2 frames) carry no encoding marker, so
  // TagLib decodes them as ISO-8859-1, as the spec says. Rippers on Chinese
  // Windows stored GBK bytes there though, which come out as mojibake:
  // "苏打绿" reads back as "ËÕ´òÂÌ".
  //
  // Such a string only contains characters below U+0100 and produces a
  // plausible Chinese title/artist once its bytes are decoded as GB18030.
  // When that holds we return the repaired text; proper Unicode text, real
  // Latin-1 names ("Édith Piaf") and plain ASCII are returned untouched.
  QString fromLegacyMojibake(const QString &text);
}

#endif // TEXTDECODE_H
