// RavenEye - Firmware fuer den M5Stack Cardputer ADV.
//
// Startmenue mit Meldungen (ntfy), Status (Uptime Kuma), Dateien (eigener Dateimanager)
// und Einstellungen. WLAN und ntfy laufen im Hintergrund weiter, egal welche App offen ist.
//
// Tasten ueberall:  ; . , /  = hoch runter links rechts     Enter = oeffnen
//                   ` (Esc) oder Del = zurueck               1-4 im Menue = direkt oeffnen

#include <M5Cardputer.h>

#include "apps.h"
#include "config.h"
#include "core.h"
#include "keys.h"
#include "ui.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// HTTPS + JSON + Dialoge brauchen mehr als die ueblichen 8 KB
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static App* current = &homeApp;
static bool dirty = true;

App* openApp(App* app) {
    current = app;
    if (app->enter) app->enter();
    dirty = true;
    return app;
}

static void fatal(const String& msg) {
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setFont(&ui::FONT);
    d.setTextColor(TFT_RED);
    d.drawString("Fehler:", 4, 4);
    d.setTextColor(TFT_WHITE);
    d.drawString(msg, 4, 24);
    d.setTextColor(TFT_DARKGREY);
    d.drawString("SD: /raveneye/config.txt", 4, 60);
    while (true) delay(1000);
}

void setup() {
    auto m5cfg = M5.config();
    M5Cardputer.begin(m5cfg, true);
    Serial.begin(115200);

    auto& d = M5Cardputer.Display;
    d.setRotation(1);
    d.setBrightness(128);
    d.setFont(&ui::FONT);
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_CYAN);
    d.drawString("RavenEye - starte ...", 4, 4);
    d.setTextColor(TFT_DARKGREY);
    d.drawString("Version " FW_VERSION, 4, 20);

    String err;
    if (!loadConfig(core::cfg, err)) fatal(err);

    ui::begin();
    core::begin();
    filesBegin();
}

void loop() {
    Keys k;
    bool pressed = keys::poll(k);
    core::tick();

    if (pressed) {
        App* before = current;
        bool stay = current->key(k);
        if (!stay && current == before && current != &homeApp) current = &homeApp;
        dirty = true;
    }
    if (current->tick) current->tick();

    if (core::takeDirty()) dirty = true;
    if (dirty) {
        current->draw();
        ui::push();
        dirty = false;
    }
    delay(5);
}
