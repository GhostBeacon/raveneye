// Bildschirm-Bausteine: Leinwand, Schriften, Kopfzeile, Listen und blockierende Dialoge.
// Dialoge halten den Hintergrund (WLAN, ntfy) ueber core::tick() am Laufen.
#pragma once

#include <M5Cardputer.h>

#include <functional>
#include <vector>

namespace ui {

extern M5Canvas canvas;  // doppelt gepuffert gegen Flackern
extern const lgfx::U8g2font FONT;
extern const lgfx::U8g2font FONT_BOLD;
extern int LINE_H;    // Zeilenhoehe der Schrift
extern int HEADER_H;  // Hoehe der Kopfzeile

// Farben "Neon Noir": schwarz, Cyan als Hauptfarbe, Magenta fuer Akzente
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
constexpr uint16_t C_BG = rgb(0, 0, 0);
constexpr uint16_t C_TEXT = rgb(0x9f, 0xb3, 0xc8);     // normaler Text
constexpr uint16_t C_BRIGHT = rgb(0xe8, 0xf6, 0xff);   // hervorgehobener Text
constexpr uint16_t C_CYAN = rgb(0x05, 0xd9, 0xe8);
constexpr uint16_t C_MAGENTA = rgb(0xff, 0x2a, 0x6d);
constexpr uint16_t C_HINT = rgb(0x5a, 0x6b, 0x85);     // Hinweise, Beschriftungen
constexpr uint16_t C_DIM = rgb(0x24, 0x30, 0x40);      // Rahmen, leere Balken
constexpr uint16_t C_OK = rgb(0x39, 0xff, 0x88);
constexpr uint16_t C_WARN = rgb(0xff, 0x8c, 0x1a);
constexpr uint16_t C_ERR = rgb(0xff, 0x2a, 0x3a);
constexpr uint16_t C_YELLOW = rgb(0xf5, 0xe6, 0x63);
constexpr uint16_t C_INFO = rgb(0x5d, 0x9f, 0xff);     // Wartung
constexpr uint16_t C_SELECT = C_CYAN;                   // Auswahlbalken

// Textfarbe in einer Liste: auf dem Auswahlbalken schwarz, sonst c
inline uint16_t ink(bool selected, uint16_t c) { return selected ? C_BG : c; }

void begin();
int width();
int height();

// Text
String fitLine(const String& s, int maxW);                 // eine Zeile, sonst "..."
std::vector<String> wrap(const String& text, int maxW);    // Zeilenumbruch nach Woertern
uint16_t prioColor(uint8_t priority);

// Rahmen eines Bildschirms
void clear();
void drawHeader(const char* title);        // Titel (in Grossbuchstaben) + Punkt, Uhrzeit, Akku, ungelesene Meldungen
void drawHint(const String& hint);         // graue Zeile ganz unten
int contentTop();                          // erste freie Zeile unter der Kopfzeile
int contentBottom(bool withHint);          // letzte freie Zeile (+1)
void push();                               // Hinweis-Banner zeichnen und anzeigen

// Scrollende Liste: haelt sel sichtbar, ruft row() fuer jede sichtbare Zeile
void drawList(int count, int sel, int& top, int y0, int y1, int rowH,
              const std::function<void(int index, int y, bool selected)>& row);

// Blockierende Dialoge
void message(const String& title, const String& body, uint16_t titleColor = C_CYAN);
bool confirm(const String& title, const String& question);
int choose(const String& title, const std::vector<String>& options, int initial = 0);  // -1 = zurueck
bool textInput(const String& prompt, String& value, bool secret = false, size_t maxLen = 64);
void viewText(const String& title, const String& body, uint16_t titleColor = C_CYAN);
void busy(const String& title, const String& text);
void progress(const String& title, const String& text, uint64_t done, uint64_t total);

}  // namespace ui
