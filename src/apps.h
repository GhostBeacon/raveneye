// Die einzelnen Bildschirme ("Apps"), erreichbar ueber das Startmenue.
#pragma once

#include <Arduino.h>

#include "keys.h"

struct App {
    const char* name;
    void (*enter)();                // beim Oeffnen aus dem Menue
    bool (*key)(const Keys& k);     // false = zurueck ins Menue
    void (*draw)();                 // Bildschirm neu zeichnen (ohne push)
    void (*tick)();                 // regelmaessig, solange offen (darf nullptr sein)
    String (*badge)();              // kurzer Zustand fuer das Menue
    void (*leave)();                // beim Zurueckgehen ins Menue (darf nullptr sein)
};

extern App homeApp;
extern App ntfyApp;
extern App statusApp;
extern App settingsApp;
extern App systemApp;

