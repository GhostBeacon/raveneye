// Text-Hilfen ohne Bildschirmbezug: UTF-8, Zeichenvorrat der Schrift, Zahlen und Zeiten.
#pragma once

#include <Arduino.h>

namespace text {

// Laenge des UTF-8-Zeichens, das bei s[i] beginnt
int utf8Len(const String& s, int i);

// Bringt Text auf den Zeichenvorrat der Schrift: ASCII, Latin-1 und – — ‘ ’ ‚ “ ” „ • … €.
// Emojis und Steuerzeichen fallen weg, Unbekanntes wird "?".
String sanitize(const String& in);

String fmtTime(uint32_t unixTime);  // "14:05" (heute) bzw. "24.09."
String fmtDecimal(double v, int decimals);  // mit Komma: 99,8
String fmtSizeMb(double mb);        // 340 KB / 1,2 MB / 3,4 GB
String fmtBytes(uint64_t bytes);

}  // namespace text
