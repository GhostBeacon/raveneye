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
    auto& c = ui::canvas;
    int rowH = (ui::contentBottom(false) - ui::contentTop()) / COUNT;
    ui::drawList(COUNT, sel, top, ui::contentTop(), ui::contentBottom(false), rowH, [&](int i, int y, bool) {
        int ty = y + (rowH - ui::LINE_H) / 2;
        c.setTextColor(ui::C_HINT);
        c.drawString(String(i + 1), 6, ty);
        c.setFont(&ui::FONT_BOLD);
        c.setTextColor(TFT_WHITE);
        c.drawString(entries[i]->name, 22, ty);
        c.setFont(&ui::FONT);
        String b = entries[i]->badge ? entries[i]->badge() : String();
        c.setTextColor(TFT_CYAN);
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
