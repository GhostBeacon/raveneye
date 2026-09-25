// Tastatur: ein Ereignis pro Tastendruck, Navigationstasten wiederholen beim Festhalten.
//
// Navigation (mit oder ohne Fn):  ; hoch   . runter   , links   / rechts
//                                 ` (Esc) oder Del = zurueck
#pragma once

#include <Arduino.h>

#include <vector>

struct Keys {
    bool up = false, down = false, left = false, right = false;
    bool enter = false, del = false, tab = false, fn = false;
    bool esc = false;   // ` (auf dem Cardputer die Esc-Taste)
    bool back = false;  // esc oder del
    bool repeat = false;
    std::vector<char> chars;  // gedrueckte Zeichen, roh (inkl. ; . , / `)

    bool has(char c) const {
        for (char x : chars)
            if (x == c) return true;
        return false;
    }
};

namespace keys {
// Liest die Tastatur (ruft M5Cardputer.update()). true = neues Ereignis in k.
// Der erste Tastendruck bei abgedunkeltem Display weckt nur und liefert false.
bool poll(Keys& k);
}
