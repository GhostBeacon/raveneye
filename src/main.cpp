// RavenEye - Firmware fuer den Cardputer ADV: ntfy-Empfaenger.
//
// Tasten:  ; / .   hoch / runter      Enter  Meldung oeffnen
//          ` / Del zurueck            m      Ton an/aus

#include <M5Cardputer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiMulti.h>
#include <time.h>

#include <deque>
#include <vector>

#include "config.h"
#include "fonts.h"
#include "ntfy.h"

static constexpr size_t MAX_MESSAGES = 40;
static constexpr uint32_t DIM_AFTER_MS = 60000;
static constexpr uint8_t BRIGHT = 128;
static constexpr uint8_t DIM = 16;
// Masse ergeben sich aus der Schrifthoehe (in setup gesetzt)
static int LINE_H = 16;
static int HEADER_H = 18;
static int ROW_H = 34;  // zwei Zeilen je Meldung

static const lgfx::U8g2font FONT(raveneye_helvR10);
static const lgfx::U8g2font FONT_BOLD(raveneye_helvB10);

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

static Config cfg;
static NtfyClient ntfy;
static WiFiMulti wifiMulti;  // nimmt das staerkste bekannte Netz
static std::deque<NtfyMessage> messages;  // neueste vorn

enum class View { List, Detail };
static View view = View::List;
static int selected = 0;
static int listTop = 0;
static int detailScroll = 0;
static bool muted = false;  // bleibt ueber Neustarts erhalten (NVS)
static Preferences prefs;
static bool dirty = true;
static uint32_t lastActivity = 0;
static bool dimmed = false;
static int lastMinute = -1;

static M5Canvas canvas(&M5Cardputer.Display);  // doppelt gepuffert gegen Flackern

// ---------- Hilfsfunktionen ----------

static bool timeValid() { return time(nullptr) > 1700000000; }

static String fmtTime(uint32_t t) {
    time_t tt = t;
    struct tm m, now;
    localtime_r(&tt, &m);
    time_t n = time(nullptr);
    localtime_r(&n, &now);
    char buf[12];
    if (m.tm_yday == now.tm_yday && m.tm_year == now.tm_year) strftime(buf, sizeof buf, "%H:%M", &m);
    else strftime(buf, sizeof buf, "%d.%m.", &m);
    return buf;
}

// Dekodiert ein UTF-8-Zeichen ab s[i], setzt i weiter; ungueltig -> U+FFFD
static uint32_t nextCodepoint(const String& s, int& i) {
    uint8_t c = s[i++];
    int extra = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (extra < 0) return 0xFFFD;
    uint32_t cp = extra == 0 ? c : c & (0x3F >> extra);
    for (int k = 0; k < extra; k++) {
        if (i >= (int)s.length() || ((uint8_t)s[i] & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | ((uint8_t)s[i++] & 0x3F);
    }
    return cp;
}

static void appendUtf8(String& out, uint32_t cp) {
    char b[3];
    int n;
    if (cp < 0x80) { b[0] = cp; n = 1; }
    else if (cp < 0x800) { b[0] = 0xC0 | (cp >> 6); b[1] = 0x80 | (cp & 0x3F); n = 2; }
    else { b[0] = 0xE0 | (cp >> 12); b[1] = 0x80 | ((cp >> 6) & 0x3F); b[2] = 0x80 | (cp & 0x3F); n = 3; }
    for (int k = 0; k < n; k++) out += b[k];
}

// Bringt Text auf den Zeichenvorrat der Schrift: ASCII, Latin-1 und
// – — ‘ ’ ‚ “ ” „ • … €. Emojis und Steuerzeichen fallen weg, Unbekanntes wird "?".
static String sanitize(const String& in) {
    String out;
    int i = 0;
    while (i < (int)in.length()) {
        uint32_t cp = nextCodepoint(in, i);
        if (cp == '\n' || (cp >= 0x20 && cp < 0x7F) || (cp >= 0xA0 && cp <= 0xFF) ||
            cp == 0x2013 || cp == 0x2014 || (cp >= 0x2018 && cp <= 0x201E) || cp == 0x2022 ||
            cp == 0x2026 || cp == 0x20AC) {
            appendUtf8(out, cp);
        } else if (cp == '\t' || cp == 0x2002 || cp == 0x2003 || cp == 0x2009 || cp == 0x202F) {
            out += ' ';
        } else if (cp == 0x2010 || cp == 0x2011 || cp == 0x2212) {
            out += '-';
        } else if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || (cp >= 0x200B && cp <= 0x200D) ||
                   (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0x2600 && cp <= 0x27BF) || cp >= 0x1F000) {
            // Steuerzeichen, Emojis und ihre Verbinder: weglassen
        } else {
            out += '?';
        }
    }
    out.trim();
    return out;
}

static uint16_t prioColor(uint8_t p) {
    switch (p) {
        case 5: return TFT_RED;
        case 4: return TFT_ORANGE;
        case 1:
        case 2: return TFT_DARKGREY;
        default: return TFT_WHITE;
    }
}

// Laenge des ersten UTF-8-Zeichens ab s[i]
static int utf8Len(const String& s, int i) {
    uint8_t c = s[i];
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    return 4;
}

// Kuerzt auf eine Zeile mit "..." (UTF-8-sicher)
static String fitLine(const String& s, int maxW) {
    String one = s;
    int nl = one.indexOf('\n');
    if (nl >= 0) one = one.substring(0, nl);
    if (canvas.textWidth(one) <= maxW) return one;
    String out;
    for (int i = 0; i < (int)one.length();) {
        int l = utf8Len(one, i);
        String next = out + one.substring(i, i + l);
        if (canvas.textWidth(next + "...") > maxW) break;
        out = next;
        i += l;
    }
    return out + "...";
}

// Zeilenumbruch nach Woertern, ueberlange Woerter werden zeichenweise getrennt
static std::vector<String> wrap(const String& text, int maxW) {
    std::vector<String> lines;
    int start = 0;
    while (start <= (int)text.length()) {
        int nl = text.indexOf('\n', start);
        String para = text.substring(start, nl < 0 ? text.length() : nl);
        start = (nl < 0) ? text.length() + 1 : nl + 1;

        String line;
        int i = 0;
        while (i < (int)para.length()) {
            int sp = para.indexOf(' ', i);
            String word = para.substring(i, sp < 0 ? para.length() : sp);
            i = (sp < 0) ? para.length() : sp + 1;
            String cand = line.isEmpty() ? word : line + " " + word;
            if (canvas.textWidth(cand) <= maxW) {
                line = cand;
                continue;
            }
            if (!line.isEmpty()) lines.push_back(line);
            line = "";
            for (int j = 0; j < (int)word.length();) {
                int l = utf8Len(word, j);
                String c = word.substring(j, j + l);
                if (canvas.textWidth(line + c) > maxW) {
                    lines.push_back(line);
                    line = "";
                }
                line += c;
                j += l;
            }
        }
        lines.push_back(line);
    }
    return lines;
}

static void beep(uint8_t prio) {
    if (muted) return;
    M5Cardputer.Speaker.tone(prio >= 4 ? 2800 : 2000, 80);
    if (prio >= 4) {
        delay(120);
        M5Cardputer.Speaker.tone(2800, 80);
    }
}

static void wake() {
    lastActivity = millis();
    if (dimmed) {
        M5Cardputer.Display.setBrightness(BRIGHT);
        dimmed = false;
    }
}

// ---------- Zeichnen ----------

static void drawHeader() {
    canvas.fillRect(0, 0, canvas.width(), HEADER_H, 0x18E3);
    canvas.setTextColor(TFT_CYAN);
    canvas.setFont(&FONT_BOLD);
    canvas.drawString("RavenEye", 4, 1);
    canvas.setFont(&FONT);

    int x = canvas.width() - 4;
    char buf[16];
    snprintf(buf, sizeof buf, "%d%%", M5Cardputer.Power.getBatteryLevel());
    canvas.setTextColor(TFT_LIGHTGREY);
    x -= canvas.textWidth(buf);
    canvas.drawString(buf, x, 1);

    if (timeValid()) {
        time_t n = time(nullptr);
        struct tm m;
        localtime_r(&n, &m);
        strftime(buf, sizeof buf, "%H:%M", &m);
        x -= canvas.textWidth(buf) + 8;
        canvas.drawString(buf, x, 1);
    }

    // Verbindungspunkt: gruen = Stream laeuft, gelb = WLAN ok, rot = kein WLAN
    uint16_t dot = TFT_RED;
    if (WiFi.isConnected()) dot = ntfy.state() == NtfyClient::State::Streaming ? TFT_GREEN : TFT_YELLOW;
    x -= 10;
    canvas.fillCircle(x, HEADER_H / 2, 3, dot);

    if (muted) {
        x -= canvas.textWidth("stumm") + 8;
        canvas.setTextColor(TFT_DARKGREY);
        canvas.drawString("stumm", x, 1);
    }
}

static void drawList() {
    const int w = canvas.width();
    const int rows = (canvas.height() - HEADER_H) / ROW_H;

    if (messages.empty()) {
        canvas.setTextColor(TFT_DARKGREY);
        const char* hint = WiFi.isConnected() ? "Keine Meldungen" : "Verbinde mit WLAN ...";
        canvas.drawString(hint, (w - canvas.textWidth(hint)) / 2, 60);
        return;
    }

    if (selected < listTop) listTop = selected;
    if (selected >= listTop + rows) listTop = selected - rows + 1;

    for (int r = 0; r < rows && listTop + r < (int)messages.size(); r++) {
        const auto& m = messages[listTop + r];
        int y = HEADER_H + r * ROW_H;
        bool sel = (listTop + r) == selected;
        if (sel) canvas.fillRect(0, y, w, ROW_H, 0x2945);

        canvas.fillRect(0, y + 2, 3, ROW_H - 4, prioColor(m.priority));
        if (!m.seen) canvas.fillCircle(w - 6, y + LINE_H / 2 + 1, 3, TFT_CYAN);

        String t = fmtTime(m.time);
        int tw = canvas.textWidth(t);
        canvas.setTextColor(TFT_DARKGREY);
        canvas.drawString(t, w - 14 - tw, y + 1);

        String head = m.title.isEmpty() ? m.topic : m.title;
        canvas.setTextColor(prioColor(m.priority));
        canvas.setFont(&FONT_BOLD);
        canvas.drawString(fitLine(head, w - 26 - tw), 7, y + 1);
        canvas.setFont(&FONT);

        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.drawString(fitLine(m.message, w - 12), 7, y + 1 + LINE_H);
    }
}

static void drawDetail() {
    if (selected >= (int)messages.size()) return;
    const auto& m = messages[selected];
    const int w = canvas.width() - 8;
    const int lineH = LINE_H;
    const int maxLines = (canvas.height() - HEADER_H - 2) / lineH;

    std::vector<String> lines;
    String head = (m.title.isEmpty() ? m.topic : m.title) + "  " + fmtTime(m.time);
    for (auto& l : wrap(head, w)) lines.push_back(l);
    size_t headLines = lines.size();
    for (auto& l : wrap(m.message, w)) lines.push_back(l);

    int maxScroll = max(0, (int)lines.size() - maxLines);
    detailScroll = constrain(detailScroll, 0, maxScroll);

    for (int i = 0; i < maxLines && detailScroll + i < (int)lines.size(); i++) {
        size_t idx = detailScroll + i;
        canvas.setTextColor(idx < headLines ? prioColor(m.priority) : TFT_WHITE);
        canvas.drawString(lines[idx], 4, HEADER_H + 2 + i * lineH);
    }
    if (maxScroll > 0) {  // Scrollbalken
        int h = canvas.height() - HEADER_H;
        int barH = max(8, h * maxLines / (int)lines.size());
        int barY = HEADER_H + (h - barH) * detailScroll / maxScroll;
        canvas.fillRect(canvas.width() - 2, barY, 2, barH, TFT_DARKGREY);
    }
}

static void render() {
    canvas.fillScreen(TFT_BLACK);
    drawHeader();
    if (view == View::List) drawList();
    else drawDetail();
    canvas.pushSprite(0, 0);
    dirty = false;
}

static void fatal(const String& msg) {
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_RED);
    d.setFont(&FONT);
    d.drawString("Fehler:", 4, 4);
    d.setTextColor(TFT_WHITE);
    d.drawString(msg, 4, 24);
    d.setTextColor(TFT_DARKGREY);
    d.drawString("SD: /raveneye/config.txt", 4, 60);
    while (true) delay(1000);
}

// ---------- Eingabe ----------

static void handleKeys() {
    if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) return;
    bool wasDimmed = dimmed;
    wake();
    if (wasDimmed) {  // erster Tastendruck weckt nur
        dirty = true;
        return;
    }

    auto st = M5Cardputer.Keyboard.keysState();
    bool up = false, down = false, back = st.del, open = st.enter;
    for (char c : st.word) {
        if (c == ';') up = true;
        else if (c == '.') down = true;
        else if (c == '`') back = true;
        else if (c == 'm') {
            muted = !muted;
            prefs.putBool("muted", muted);
        }
    }

    if (view == View::List) {
        if (up && selected > 0) selected--;
        if (down && selected + 1 < (int)messages.size()) selected++;
        if (open && selected < (int)messages.size()) {
            messages[selected].seen = true;
            detailScroll = 0;
            view = View::Detail;
        }
    } else {
        if (up) detailScroll--;
        if (down) detailScroll++;
        if (back) view = View::List;
    }
    dirty = true;
}

// ---------- Setup / Loop ----------

void setup() {
    auto m5cfg = M5.config();
    M5Cardputer.begin(m5cfg, true);
    Serial.begin(115200);

    auto& d = M5Cardputer.Display;
    d.setRotation(1);
    d.setBrightness(BRIGHT);
    d.setFont(&FONT);
    d.fillScreen(TFT_BLACK);
    d.setTextColor(TFT_CYAN);
    d.drawString("RavenEye - starte ...", 4, 4);
    d.setTextColor(TFT_DARKGREY);
    d.drawString("Version " FW_VERSION, 4, 20);

    String err;
    if (!loadConfig(cfg, err)) fatal(err);

    canvas.setColorDepth(16);
    canvas.createSprite(d.width(), d.height());
    canvas.setFont(&FONT);
    LINE_H = canvas.fontHeight();
    HEADER_H = LINE_H + 2;
    ROW_H = 2 * LINE_H + 2;
    canvas.setTextDatum(top_left);

    M5Cardputer.Speaker.setVolume(96);
    prefs.begin("raveneye", false);
    muted = prefs.getBool("muted", false);

    WiFi.mode(WIFI_STA);
    for (auto& n : cfg.wifis) wifiMulti.addAP(n.ssid.c_str(), n.pass.c_str());
    configTzTime(TZ_BERLIN, "pool.ntp.org", "time.cloudflare.com");

    ntfy.begin(cfg.ntfyServer, cfg.ntfyTopics, [](NtfyMessage&& m) {
        m.title = sanitize(m.title);
        m.message = sanitize(m.message);
        for (auto& e : messages)
            if (e.id == m.id) return;
        // beim Start nachgeladene Meldungen (aelter als 2 min) klingeln nicht
        bool fresh = timeValid() && time(nullptr) - (time_t)m.time < 120;
        if (fresh) {
            wake();
            beep(m.priority);
        } else {
            m.seen = true;
        }
        messages.push_front(std::move(m));
        if (messages.size() > MAX_MESSAGES) messages.pop_back();
        // Auswahl bleibt auf derselben Meldung
        if (view == View::Detail || selected > 0) selected = min(selected + 1, (int)messages.size() - 1);
        dirty = true;
    });

    lastActivity = millis();
}

void loop() {
    M5Cardputer.update();
    handleKeys();

    // TLS braucht eine gueltige Uhrzeit fuer die Zertifikatspruefung
    ntfy.poll(WiFi.isConnected() && timeValid());

    static NtfyClient::State lastState = NtfyClient::State::Idle;
    static bool lastWifi = false;
    if (ntfy.state() != lastState || WiFi.isConnected() != lastWifi) {
        lastState = ntfy.state();
        lastWifi = WiFi.isConnected();
        dirty = true;
    }
    if (timeValid()) {
        time_t n = time(nullptr);
        int minute = n / 60;
        if (minute != lastMinute) {
            lastMinute = minute;
            dirty = true;
        }
    }

    if (!dimmed && millis() - lastActivity > DIM_AFTER_MS) {
        M5Cardputer.Display.setBrightness(DIM);
        dimmed = true;
    }

    if (dirty) render();
    // Ohne Verbindung alle 10 s scannen und das staerkste bekannte Netz nehmen (blockiert kurz)
    static uint32_t lastWifiTry = 0;
    if (!WiFi.isConnected() && (lastWifiTry == 0 || millis() - lastWifiTry > 10000)) {
        wifiMulti.run(8000);
        lastWifiTry = millis();
        dirty = true;
    }
    delay(10);
}
