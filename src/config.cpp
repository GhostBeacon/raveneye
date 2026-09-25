#include "config.h"

#include <SD.h>
#include <SPI.h>

// SD-Pins des Cardputer ADV
static constexpr int SD_SCK = 40;
static constexpr int SD_MISO = 39;
static constexpr int SD_MOSI = 14;
static constexpr int SD_CS = 12;

static const char* CONFIG_PATH = "/raveneye/config.txt";

// ntfy-Topics: [-_A-Za-z0-9]{1,64}, mehrere mit Komma
static bool validTopics(const String& topics) {
    if (topics.isEmpty()) return false;
    int len = 0;
    for (char c : topics) {
        if (c == ',') {
            if (len == 0) return false;
            len = 0;
        } else if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            if (++len > 64) return false;
        } else {
            return false;
        }
    }
    return len > 0;
}

bool loadConfig(Config& cfg, String& error) {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 25000000)) {
        error = "Keine SD-Karte";
        return false;
    }
    File f = SD.open(CONFIG_PATH);
    if (!f) {
        error = String(CONFIG_PATH) + " fehlt";
        return false;
    }
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.isEmpty() || line[0] == '#') continue;
        int eq = line.indexOf('=');
        if (eq < 0) continue;
        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();
        if (key == "wifi_ssid") cfg.wifiSsid = val;
        else if (key == "wifi_pass") cfg.wifiPass = val;
        else if (key == "ntfy_server") cfg.ntfyServer = val;
        else if (key == "ntfy_topics") cfg.ntfyTopics = val;
    }
    f.close();

    if (cfg.wifiSsid.isEmpty()) {
        error = "wifi_ssid fehlt";
        return false;
    }
    if (!validTopics(cfg.ntfyTopics)) {
        error = "ntfy_topics ungueltig";
        return false;
    }
    return true;
}
