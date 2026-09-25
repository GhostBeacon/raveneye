#include "text.h"

#include <time.h>

namespace text {

int utf8Len(const String& s, int i) {
    uint8_t c = s[i];
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    return 4;
}

// Dekodiert ein UTF-8-Zeichen ab s[i], setzt i weiter; ungueltig -> U+FFFD
static uint32_t nextCodepoint(const String& s, int& i) {
    uint8_t c = s[i++];
    int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (extra < 0) return 0xFFFD;
    uint32_t cp = extra == 0 ? c : c & (0x3F >> extra);
    for (int k = 0; k < extra; k++) {
        if (i >= (int)s.length() || ((uint8_t)s[i] & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | ((uint8_t)s[i++] & 0x3F);
    }
    return cp;
}

static void appendUtf8(String& out, uint32_t cp) {
    char b[3];
    int n;
    if (cp < 0x80) { b[0] = cp; n = 1; }
    else if (cp < 0x800) { b[0] = 0xC0 | (cp >> 6); b[1] = 0x80 | (cp & 0x3F); n = 2; }
    else { b[0] = 0xE0 | (cp >> 12); b[1] = 0x80 | ((cp >> 6) & 0x3F); b[2] = 0x80 | (cp & 0x3F); n = 3; }
    for (int k = 0; k < n; k++) out += b[k];
}

String sanitize(const String& in) {
    String out;
    int i = 0;
    while (i < (int)in.length()) {
        uint32_t cp = nextCodepoint(in, i);
        if (cp == '\n' || (cp >= 0x20 && cp < 0x7F) || (cp >= 0xA0 && cp <= 0xFF) ||
            cp == 0x2013 || cp == 0x2014 || (cp >= 0x2018 && cp <= 0x201E) || cp == 0x2022 ||
            cp == 0x2026 || cp == 0x20AC) {
            appendUtf8(out, cp);
        } else if (cp == '\t' || cp == 0x2002 || cp == 0x2003 || cp == 0x2009 || cp == 0x202F) {
            out += ' ';
        } else if (cp == 0x2010 || cp == 0x2011 || cp == 0x2212) {
            out += '-';
        } else if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || (cp >= 0x200B && cp <= 0x200D) ||
                   (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0x2600 && cp <= 0x27BF) || cp >= 0x1F000) {
            // Steuerzeichen, Emojis und ihre Verbinder: weglassen
        } else {
            out += '?';
        }
    }
    out.trim();
    return out;
}

String fmtTime(uint32_t t) {
    time_t tt = t;
    struct tm m, now;
    localtime_r(&tt, &m);
    time_t n = time(nullptr);
    localtime_r(&n, &now);
    char buf[12];
    if (m.tm_yday == now.tm_yday && m.tm_year == now.tm_year) strftime(buf, sizeof buf, "%H:%M", &m);
    else strftime(buf, sizeof buf, "%d.%m.", &m);
    return buf;
}

String fmtDecimal(double v, int decimals) {
    String s(v, decimals);
    s.replace('.', ',');
    return s;
}

String fmtSizeMb(double mb) {
    if (mb < 1.0) return String((int)round(mb * 1024)) + " KB";
    if (mb < 1024.0) return fmtDecimal(mb, mb < 10 ? 1 : 0) + " MB";
    return fmtDecimal(mb / 1024.0, 1) + " GB";
}

String fmtBytes(uint64_t bytes) { return fmtSizeMb(bytes / (1024.0 * 1024.0)); }

}  // namespace text
