// Einstellungen und Geraeteinfo. Enter aendert bzw. zeigt Details.

#include <SD.h>
#include <WiFi.h>

#include "apps.h"
#include "core.h"
#include "text.h"
#include "ui.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static int sel = 0, top = 0;
static uint64_t sdTotal = 0, sdUsed = 0;  // beim Oeffnen gelesen (usedBytes kann dauern)

static void enter() {
    sdTotal = SD.cardSize() ? SD.totalBytes() : 0;
    sdUsed = sdTotal ? SD.usedBytes() : 0;
}

static const uint8_t BRIGHT_STEPS[] = {32, 64, 128, 192, 255};
static const uint32_t DIM_STEPS[] = {30000, 60000, 120000, 300000, 0};

enum Row { R_SOUND, R_BRIGHT, R_DIM, R_WIFI, R_CLOUD, R_SD, R_SYSTEM, R_RESTART, R_COUNT };

static String label(int r) {
    switch (r) {
        case R_SOUND: return "Ton";
        case R_BRIGHT: return "Helligkeit";
        case R_DIM: return "Abdunkeln nach";
        case R_WIFI: return "WLAN";
        case R_CLOUD: return "Dateien";
        case R_SD: return "SD-Karte";
        case R_SYSTEM: return "System";
        case R_RESTART: return "Neu starten";
    }
    return "";
}

static String value(int r) {
    auto& s = core::settings;
    switch (r) {
        case R_SOUND: return s.muted ? "aus" : "an";
        case R_BRIGHT: return String((s.brightness * 100 + 127) / 255) + " %";
        case R_DIM:
            if (!s.dimAfterMs) return "nie";
            return s.dimAfterMs < 60000 ? String(s.dimAfterMs / 1000) + " s" : String(s.dimAfterMs / 60000) + " min";
        case R_WIFI: return core::online() ? text::sanitize(core::wifiSsid()) : String("getrennt");
        case R_CLOUD: {
            String u = filesUser();
            return u.isEmpty() ? String("abgemeldet") : text::sanitize(u);
        }
        case R_SD: return sdTotal ? text::fmtBytes(sdTotal - sdUsed) + " frei" : String("-");
        case R_SYSTEM: return "v" FW_VERSION;
        default: return "";
    }
}

static void draw() {
    ui::clear();
    ui::drawHeader("Einstellungen");
    auto& c = ui::canvas;
    const int w = c.width(), rowH = ui::LINE_H + 1;
    ui::drawList(R_COUNT, sel, top, ui::contentTop(), ui::contentBottom(false), rowH, [&](int i, int y, bool) {
        c.setTextColor(TFT_WHITE);
        c.drawString(label(i), 6, y);
        String v = value(i);
        c.setTextColor(TFT_CYAN);
        v = ui::fitLine(v, w / 2);
        c.drawString(v, w - 8 - c.textWidth(v), y);
    });
}

template <typename T, size_t N>
static T nextStep(const T (&steps)[N], T current) {
    for (size_t i = 0; i < N; i++)
        if (steps[i] == current) return steps[(i + 1) % N];
    return steps[0];
}

static void activate(int r) {
    auto& s = core::settings;
    switch (r) {
        case R_SOUND:
            s.muted = !s.muted;
            core::saveSettings();
            break;
        case R_BRIGHT:
            s.brightness = nextStep(BRIGHT_STEPS, s.brightness);
            core::applyBrightness();
            core::saveSettings();
            break;
        case R_DIM:
            s.dimAfterMs = nextStep(DIM_STEPS, s.dimAfterMs);
            core::saveSettings();
            break;
        case R_WIFI: {
            String t;
            if (core::online()) {
                t = "Netz: " + text::sanitize(core::wifiSsid()) + "\nSignal: " + String(core::wifiRssi()) + " dBm\nIP: " +
                    WiFi.localIP().toString();
            } else {
                t = "Nicht verbunden.";
            }
            t += "\n\nBekannte Netze (config.txt):";
            for (auto& n : core::cfg.wifis) t += "\n- " + text::sanitize(n.ssid);
            ui::message("WLAN", t);
            break;
        }
        case R_CLOUD:
            if (filesUser().isEmpty()) {
                ui::message("Dateien", "Nicht angemeldet. Anmelden unter Start > Dateien.");
            } else if (ui::confirm("Dateien", "Abmelden? Das Token wird auf dem Server gelöscht.")) {
                filesLogout();
            }
            break;
        case R_SD:
            enter();
            if (sdTotal) {
                ui::message("SD-Karte", "Größe: " + text::fmtBytes(sdTotal) + "\nBelegt: " + text::fmtBytes(sdUsed) +
                                            "\nFrei: " + text::fmtBytes(sdTotal - sdUsed) +
                                            "\n\nDownloads: /raveneye/downloads");
            }
            break;
        case R_SYSTEM:
            ui::message("System", "RavenEye " FW_VERSION "\nRAM frei: " + text::fmtBytes(ESP.getFreeHeap()) +
                                      " (kleinster Stand " + text::fmtBytes(ESP.getMinFreeHeap()) + ")\nLaufzeit: " +
                                      String(millis() / 60000) + " min\nAkku: " + String(M5Cardputer.Power.getBatteryLevel()) + " %");
            break;
        case R_RESTART:
            if (ui::confirm("Neu starten", "Jetzt neu starten? (z. B. nach Änderung der config.txt)")) ESP.restart();
            break;
    }
}

static bool key(const Keys& k) {
    if (k.back) return false;
    if (k.up && sel > 0) sel--;
    else if (k.down && sel + 1 < R_COUNT) sel++;
    else if (k.enter) activate(sel);
    return true;
}

App settingsApp = {"Einstellungen", enter, key, draw, nullptr, nullptr};
