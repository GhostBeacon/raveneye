#include "core.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <time.h>

#include "text.h"

namespace core {

Config cfg;
Settings settings;
Preferences prefs;
NtfyClient ntfy;
std::deque<NtfyMessage> messages;
bool ntfyVisible = false;
bool keepAwake = false;

static constexpr size_t MAX_MESSAGES = 40;
static const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

static bool dirty = true;
static bool dimmed = false;
static uint32_t lastActivity = 0;
static int lastMinute = -1;
static String toastText;
static uint32_t toastUntil = 0;

// ---------- WLAN: nicht blockierend (Scan im Hintergrund, staerkstes bekanntes Netz) ----------

enum class WifiState { Idle, Scanning, Connecting, Connected };
static WifiState wifiState = WifiState::Idle;
static uint32_t wifiNextTry = 0;
static uint32_t wifiConnectStart = 0;

static void wifiTick() {
    uint32_t now = millis();
    switch (wifiState) {
        case WifiState::Connected:
            if (!WiFi.isConnected()) {
                wifiState = WifiState::Idle;
                wifiNextTry = now;
                dirty = true;
            }
            break;

        case WifiState::Idle:
            if (WiFi.isConnected()) {
                wifiState = WifiState::Connected;
                dirty = true;
            } else if ((int32_t)(now - wifiNextTry) >= 0) {
                WiFi.scanNetworks(true);  // asynchron
                wifiState = WifiState::Scanning;
            }
            break;

        case WifiState::Scanning: {
            int n = WiFi.scanComplete();
            if (n == WIFI_SCAN_RUNNING) break;
            int best = -1, bestRssi = -1000;
            for (int i = 0; i < n; i++) {
                for (size_t w = 0; w < cfg.wifis.size(); w++) {
                    if (WiFi.SSID(i) == cfg.wifis[w].ssid && WiFi.RSSI(i) > bestRssi) {
                        best = w;
                        bestRssi = WiFi.RSSI(i);
                    }
                }
            }
            WiFi.scanDelete();
            if (best >= 0) {
                WiFi.begin(cfg.wifis[best].ssid.c_str(), cfg.wifis[best].pass.c_str());
                wifiState = WifiState::Connecting;
                wifiConnectStart = now;
            } else {
                wifiState = WifiState::Idle;
                wifiNextTry = now + 10000;
            }
            break;
        }

        case WifiState::Connecting:
            if (WiFi.isConnected()) {
                wifiState = WifiState::Connected;
                dirty = true;
            } else if (now - wifiConnectStart > 15000) {
                WiFi.disconnect();
                wifiState = WifiState::Idle;
                wifiNextTry = now + 3000;
            }
            break;
    }
}

// ---------- Meldungen ----------

static void onMessage(NtfyMessage&& m) {
    for (auto& e : messages)
        if (e.id == m.id) return;
    m.title = text::sanitize(m.title);
    m.message = text::sanitize(m.message);

    // Beim Start nachgeladene Meldungen (aelter als 2 min) klingeln nicht
    bool fresh = timeValid() && time(nullptr) - (time_t)m.time < 120;
    if (fresh) {
        wake();
        beep(m.priority);
        if (!ntfyVisible) toast("Neu: " + (m.title.isEmpty() ? m.message : m.title));
    } else {
        m.seen = true;
    }
    messages.push_front(std::move(m));
    if (messages.size() > MAX_MESSAGES) messages.pop_back();
    dirty = true;
}

// ---------- oeffentlich ----------

void begin() {
    prefs.begin("raveneye", false);
    settings.muted = prefs.getBool("muted", false);
    settings.brightness = prefs.getUChar("bright", 128);
    settings.dimAfterMs = prefs.getULong("dim", 60000);
    applyBrightness();
    M5Cardputer.Speaker.setVolume(96);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // wifiTick() waehlt selbst das staerkste Netz
    configTzTime(TZ_BERLIN, "pool.ntp.org", "time.cloudflare.com");
    ntfy.begin(cfg.ntfyServer, cfg.ntfyTopics, onMessage);
    lastActivity = millis();
}

void tick() {
    wifiTick();
    // TLS braucht eine gueltige Uhrzeit fuer die Zertifikatspruefung
    static NtfyClient::State lastState = NtfyClient::State::Idle;
    ntfy.poll(online() && timeValid());
    if (ntfy.state() != lastState) {
        lastState = ntfy.state();
        dirty = true;
    }

    if (timeValid()) {
        int minute = time(nullptr) / 60;
        if (minute != lastMinute) {
            lastMinute = minute;
            dirty = true;
        }
    }
    if (toastUntil && (int32_t)(millis() - toastUntil) >= 0) {
        toastUntil = 0;
        toastText = "";
        dirty = true;
    }
    if (keepAwake) lastActivity = millis();
    if (!dimmed && settings.dimAfterMs && millis() - lastActivity > settings.dimAfterMs) {
        M5Cardputer.Display.setBrightness(max(4, settings.brightness / 8));
        dimmed = true;
    }
}

void saveSettings() {
    prefs.putBool("muted", settings.muted);
    prefs.putUChar("bright", settings.brightness);
    prefs.putULong("dim", settings.dimAfterMs);
}

void applyBrightness() {
    M5Cardputer.Display.setBrightness(settings.brightness);
    dimmed = false;
}

bool timeValid() { return time(nullptr) > 1700000000; }
bool online() { return WiFi.isConnected(); }
String wifiSsid() { return WiFi.isConnected() ? WiFi.SSID() : String(); }
int wifiRssi() { return WiFi.isConnected() ? WiFi.RSSI() : 0; }

int unread() {
    int n = 0;
    for (auto& m : messages) n += !m.seen;
    return n;
}

bool wake() {
    lastActivity = millis();
    if (!dimmed) return false;
    applyBrightness();
    dirty = true;
    return true;
}

void beep(uint8_t priority) {
    if (settings.muted) return;
    M5Cardputer.Speaker.tone(priority >= 4 ? 2800 : 2000, 80);
    if (priority >= 4) {
        delay(120);
        M5Cardputer.Speaker.tone(2800, 80);
    }
}

void markDirty() { dirty = true; }

bool takeDirty() {
    bool d = dirty;
    dirty = false;
    return d;
}

void toast(const String& t, uint32_t ms) {
    toastText = t;
    toastUntil = millis() + ms;
    if (toastUntil == 0) toastUntil = 1;
    dirty = true;
}

String currentToast() { return toastText; }

}  // namespace core
