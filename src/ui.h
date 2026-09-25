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

// Farben
constexpr uint16_t C_HEADER = 0x18E3;
constexpr uint16_t C_SELECT = 0x2945;
constexpr uint16_t C_HINT = TFT_DARKGREY;

void begin();
int width();
int height();

// Text
String fitLine(const String& s, int maxW);                 // eine Zeile, sonst "..."
std::vector<String> wrap(const String& text, int maxW);    // Zeilenumbruch nach Woertern
uint16_t prioColor(uint8_t priority);

// Rahmen eines Bildschirms
void clear();
void drawHeader(const char* title);        // Titel + Punkt, Uhrzeit, Akku, ungelesene Meldungen
void drawHint(const String& hint);         // graue Zeile ganz unten
int contentTop();                          // erste freie Zeile unter der Kopfzeile
int contentBottom(bool withHint);          // letzte freie Zeile (+1)
void push();                               // Hinweis-Banner zeichnen und anzeigen

// Scrollende Liste: haelt sel sichtbar, ruft row() fuer jede sichtbare Zeile
void drawList(int count, int sel, int& top, int y0, int y1, int rowH,
              const std::function<void(int index, int y, bool selected)>& row);

// Blockierende Dialoge
void message(const String& title, const String& body, uint16_t titleColor = TFT_CYAN);
bool confirm(const String& title, const String& question);
int choose(const String& title, const std::vector<String>& options, int initial = 0);  // -1 = zurueck
bool textInput(const String& prompt, String& value, bool secret = false, size_t maxLen = 64);
void viewText(const String& title, const String& body, uint16_t titleColor = TFT_CYAN);
void busy(const String& title, const String& text);
void progress(const String& title, const String& text, uint64_t done, uint64_t total);

}  // namespace ui
