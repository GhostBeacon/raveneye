// Startmenue: Meldungen, Status, System, Einstellungen (auch ueber die Tasten 1-4)

#include "apps.h"
#include "core.h"
#include "ui.h"

App* openApp(App* app);  // main.cpp

static App* const entries[] = {&ntfyApp, &statusApp, &systemApp, &settingsApp};
static constexpr int COUNT = sizeof(entries) / sizeof(entries[0]);
static int sel = 0, top = 0;

static void draw() {
    ui::clear();
    ui::drawHeader("RavenEye");
    ui::drawHint("; .  wählen   Enter  öffnen   m  Ton");
    auto& c = ui::canvas;
    const int y0 = ui::contentTop() + 2, y1 = ui::contentBottom(true) - 2;
    int rowH = (y1 - y0) / COUNT;
    ui::drawList(COUNT, sel, top, y0, y1, rowH, [&](int i, int y, bool s) {
        int ty = y + (rowH - ui::LINE_H) / 2 + 2;
        c.setFont(&ui::FONT_BOLD);
        c.setTextColor(ui::ink(s, ui::C_TEXT));
        String name = entries[i]->name;
        name.toUpperCase();
        c.drawString("0" + String(i + 1) + "  " + name, 10, ty);
        c.setFont(&ui::FONT);
        String b = entries[i]->badge ? entries[i]->badge() : String();
        // Ungelesene Meldungen in Magenta, sonst unauffaellig
        bool alert = entries[i] == &ntfyApp && core::unread() > 0;
        c.setTextColor(ui::ink(s, alert ? ui::C_MAGENTA : ui::C_HINT));
        c.drawString(b, c.width() - 8 - c.textWidth(b), ty);
    });
}

static bool key(const Keys& k) {
    if (k.has('m')) {  // Ton an/aus
        core::settings.muted = !core::settings.muted;
        core::saveSettings();
    }
    if (k.up && sel > 0) sel--;
    else if (k.down && sel + 1 < COUNT) sel++;
    else if (k.enter) openApp(entries[sel]);
    for (char ch : k.chars)
        if (ch >= '1' && ch < '1' + COUNT) {
            sel = ch - '1';
            openApp(entries[sel]);
        }
    return true;  // das Menue verlaesst man nicht
}

App homeApp = {"Start", nullptr, key, draw, nullptr, nullptr};
