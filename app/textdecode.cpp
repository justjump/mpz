#include "textdecode.h"

#include <QByteArray>

#include <optional>
#include <string>

#if defined(Q_OS_WIN)
  #include <windows.h>
#elif defined(MPZ_HAVE_ICONV)
  #include <iconv.h>
#endif

namespace {
  bool isCjkOrPunctuation(ushort u) {
    return (u >= 0x2000 && u <= 0x206F)   // dashes, curly quotes, ellipsis
        || (u >= 0x2150 && u <= 0x218F)   // Roman numerals ("Volume IV" as Ⅳ)
        || (u >= 0x2190 && u <= 0x21FF)   // arrows
        || (u >= 0x2460 && u <= 0x24FF)   // enclosed numbers, ①
        || (u >= 0x25A0 && u <= 0x27BF)   // shapes, stars, music notes
        || (u >= 0x3000 && u <= 0x303F)   // CJK punctuation
        || (u >= 0x3400 && u <= 0x4DBF)   // CJK extension A
        || (u >= 0x4E00 && u <= 0x9FFF)   // CJK unified ideographs
        || (u >= 0xF900 && u <= 0xFAFF)   // CJK compatibility ideographs
        || (u >= 0xFF01 && u <= 0xFF60)   // fullwidth forms
        || (u >= 0xFF61 && u <= 0xFFDC)   // halfwidth forms
        || u == 0x00B7;                   // middle dot, very common in artist names
  }

  // Tells a real decode from a lucky one: the text has to read like Chinese
  // metadata, not like a Latin name that happened to swallow one ideograph.
  bool looksLikeChinese(const QString &text) {
    int ideographs = 0;
    for (const QChar ch : text) {
      const ushort u = ch.unicode();
      if (u < 0x20 && u != u'\t') {
        return false;
      }
      if (u < 0x80) {
        continue;
      }
      if (!isCjkOrPunctuation(u)) {
        return false;
      }
      ++ideographs;
    }
    // A single ideograph is what a wrong guess looks like: two Latin-1 bytes
    // such as "Ça" decode cleanly into one. Real Chinese metadata has at least
    // two.
    return ideographs >= 2;
  }

  // GB2312 keeps every character in two bytes above 0xA0, and that is what
  // Chinese rippers wrote. A Latin letter following an accented one ("Mötley")
  // is below 0xA1, so this rejects Latin names that happen to decode into a
  // couple of ideographs.
  bool usesGb2312Pairs(const QByteArray &bytes) {
    int i = 0;
    const int size = static_cast<int>(bytes.size());
    while (i < size) {
      const auto b = static_cast<unsigned char>(bytes.at(i));
      if (b < 0x80) {
        ++i;
        continue;
      }
      if (i + 1 >= size) {
        return false;
      }
      if (b < 0xA1 || static_cast<unsigned char>(bytes.at(i + 1)) < 0xA1) {
        return false;
      }
      i += 2;
    }
    return true;
  }

  bool isValidUtf8(const QByteArray &bytes) {
    int i = 0;
    const int size = static_cast<int>(bytes.size());
    while (i < size) {
      const auto b = static_cast<unsigned char>(bytes.at(i));
      int extra = 0;
      if (b < 0x80) {
        ++i;
        continue;
      } else if (b >= 0xC2 && b <= 0xDF) {
        extra = 1;
      } else if (b >= 0xE0 && b <= 0xEF) {
        extra = 2;
      } else if (b >= 0xF0 && b <= 0xF4) {
        extra = 3;
      } else {
        return false;
      }
      if (i + extra >= size) {
        return false;
      }
      for (int k = 1; k <= extra; ++k) {
        if ((static_cast<unsigned char>(bytes.at(i + k)) & 0xC0) != 0x80) {
          return false;
        }
      }
      i += extra + 1;
    }
    return true;
  }

  // Text that a UTF-8-aware tool stored in a Latin-1 field: it reads back as
  // "HÃ©lÃ¨ne", so the UTF-8 round trip is the repair.
  bool looksLikeUtf8Text(const QString &text) {
    bool has_non_ascii = false;
    for (const QChar ch : text) {
      const ushort u = ch.unicode();
      if (u < 0x20 && u != u'\t') {
        return false;
      }
      if (u < 0x80) {
        continue;
      }
      has_non_ascii = true;
      const bool latin = (u >= 0xC0 && u <= 0x24F) || (u >= 0x1E00 && u <= 0x1EFF);
      if (!latin && !isCjkOrPunctuation(u)) {
        return false;
      }
    }
    return has_non_ascii;
  }

  std::optional<QString> decodeGb18030(const QByteArray &bytes) {
    if (bytes.isEmpty()) {
      return std::nullopt;
    }
#if defined(Q_OS_WIN)
    constexpr UINT gb18030 = 54936;
    const int length = MultiByteToWideChar(gb18030, 0, bytes.constData(), bytes.size(), nullptr, 0);
    if (length <= 0) {
      return std::nullopt;
    }
    std::wstring wide(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(gb18030, 0, bytes.constData(), bytes.size(), wide.data(), length) != length) {
      return std::nullopt;
    }
    return QString::fromWCharArray(wide.data(), length);
#elif defined(MPZ_HAVE_ICONV)
    iconv_t cd = iconv_open("UTF-8", "GB18030");
    if (cd == reinterpret_cast<iconv_t>(-1)) {
      return std::nullopt;
    }
    QByteArray out(static_cast<int>(bytes.size()) * 4 + 8, '\0');
    char *in = const_cast<char *>(bytes.constData());
    size_t in_left = static_cast<size_t>(bytes.size());
    char *out_ptr = out.data();
    size_t out_left = static_cast<size_t>(out.size());
    const size_t result = iconv(cd, &in, &in_left, &out_ptr, &out_left);
    iconv_close(cd);
    if (result == static_cast<size_t>(-1) || in_left != 0) {
      return std::nullopt;
    }
    out.truncate(out.size() - static_cast<int>(out_left));
    const QString decoded = QString::fromUtf8(out);
    if (decoded.contains(QChar::ReplacementCharacter)) {
      return std::nullopt;
    }
    return decoded;
#else
    Q_UNUSED(bytes);
    return std::nullopt;
#endif
  }
}

QString TextDecode::fromLegacyMojibake(const QString &text) {
  if (text.isEmpty()) {
    return text;
  }

  bool has_high_byte = false;
  for (const QChar ch : text) {
    const ushort u = ch.unicode();
    if (u > 0xFF) {
      return text; // already real Unicode
    }
    if (u >= 0x80) {
      has_high_byte = true;
    }
  }
  if (!has_high_byte) {
    return text;
  }

  QByteArray bytes;
  bytes.reserve(text.size());
  for (const QChar ch : text) {
    bytes.append(static_cast<char>(ch.unicode()));
  }
  while (bytes.endsWith(' ')) {
    bytes.chop(1);
  }

  // UTF-8 written into the Latin-1 field by a UTF-8-aware tool. Chinese GBK
  // pairs are almost never valid UTF-8 (their second byte is above 0xBF), so
  // this cannot steal a GBK tag.
  if (isValidUtf8(bytes)) {
    const QString decoded = QString::fromUtf8(bytes);
    if (looksLikeUtf8Text(decoded)) {
      return decoded;
    }
  }

  // Chinese GBK/GB18030. ID3v1 fields are fixed width, so a multibyte
  // character cut in half at the end of the field is normal: drop the stray
  // bytes before giving up.
  for (int attempt = 0; attempt < 3 && !bytes.isEmpty(); ++attempt) {
    if (const std::optional<QString> decoded = decodeGb18030(bytes)) {
      if (looksLikeChinese(*decoded) && usesGb2312Pairs(bytes)) {
        return *decoded;
      }
      return text;
    }
    if (static_cast<unsigned char>(bytes.back()) < 0x80) {
      break;
    }
    bytes.chop(1);
  }
  return text;
}
