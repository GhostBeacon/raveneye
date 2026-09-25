#include "net.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "certs.h"
#include "core.h"

namespace net {

static constexpr uint16_t TIMEOUT_MS = 15000;

static bool open(HTTPClient& http, WiFiClientSecure& client, const String& url, const String& bearer) {
    client.setCACert(ROOT_CAS);
    client.setTimeout(TIMEOUT_MS / 1000);
    http.setTimeout(TIMEOUT_MS);
    http.setConnectTimeout(TIMEOUT_MS);
    http.useHTTP10(true);  // kein chunked Transfer -> Antwort laesst sich direkt streamen
    http.setUserAgent("raveneye");
    if (!http.begin(client, url)) return false;
    if (!bearer.isEmpty()) http.addHeader("Authorization", "Bearer " + bearer);
    return true;
}

// Fehlertext aus Status + JSON-Fehlerkoerper der API ({"error": ..., "message": ...})
static void fillError(Result& r, HTTPClient& http) {
    if (r.status <= 0) {
        r.error = "Keine Verbindung (" + HTTPClient::errorToString(r.status) + ")";
        return;
    }
    if (r.ok()) return;
    String body = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, body)) {
        r.code = doc["error"] | "";
        r.error = doc["message"] | "";
    }
    if (r.error.isEmpty()) r.error = "Serverfehler " + String(r.status);
}

static Result precheck(const String& url) {
    Result r;
    if (!core::online()) {
        r.status = -100;
        r.error = "Kein WLAN";
    } else if (!core::timeValid()) {
        r.status = -101;
        r.error = "Uhrzeit noch nicht gestellt (NTP)";
    } else if (!url.startsWith("https://")) {
        r.status = -102;
        r.error = "Adresse muss mit https:// beginnen";
    }
    return r;
}

Result getJson(const String& url, const String& bearer, JsonDocument& out, const JsonDocument* filter) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    if (!open(http, client, url, bearer)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    r.status = http.GET();
    if (r.ok()) {
        DeserializationError e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
                                        : deserializeJson(out, http.getStream());
        if (e) {
            r.status = -104;
            r.error = String("Antwort unlesbar (") + e.c_str() + ")";
        }
    } else {
        fillError(r, http);
    }
    http.end();
    return r;
}

Result postJson(const String& url, const String& bearer, const JsonDocument& body, JsonDocument& out) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    if (!open(http, client, url, bearer)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    http.addHeader("Content-Type", "application/json");
    String payload;
    serializeJson(body, payload);
    r.status = http.POST(payload);
    if (r.ok()) {
        if (http.getSize() != 0) deserializeJson(out, http.getStream());  // 204 = leer
    } else {
        fillError(r, http);
    }
    // Passwoerter nicht laenger als noetig im RAM halten
    for (unsigned i = 0; i < payload.length(); i++) payload.setCharAt(i, 0);
    http.end();
    return r;
}

// Liest den Antwortkoerper stueckweise; sink() bekommt jedes Stueck.
static bool readBody(HTTPClient& http, const std::function<bool(const uint8_t*, size_t)>& sink,
                     const Progress& progress, Result& r) {
    int total = http.getSize();  // -1 = unbekannt
    WiFiClient* s = http.getStreamPtr();
    uint8_t buf[1024];
    uint64_t done = 0;
    uint32_t lastData = millis(), lastDraw = 0;
    while (http.connected() || s->available()) {
        if (total >= 0 && done >= (uint64_t)total) break;
        int n = s->available();
        if (n <= 0) {
            if (millis() - lastData > TIMEOUT_MS) {
                r.status = -105;
                r.error = "Zeitueberschreitung";
                return false;
            }
            core::tick();
            delay(2);
            continue;
        }
        n = s->readBytes(buf, min(n, (int)sizeof buf));
        lastData = millis();
        if (!sink(buf, n)) return true;  // Empfaenger will nichts mehr
        done += n;
        if (progress && millis() - lastDraw > 200) {
            progress(done, total < 0 ? 0 : total);
            lastDraw = millis();
        }
    }
    if (total >= 0 && done < (uint64_t)total) {
        r.status = -106;
        r.error = "Verbindung abgebrochen";
        return false;
    }
    if (progress) progress(done, total < 0 ? done : total);
    return true;
}

Result download(const String& url, const String& bearer, fs::File& file, const Progress& progress) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    if (!open(http, client, url, bearer)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    r.status = http.GET();
    if (r.ok()) {
        bool writeFailed = false;
        readBody(http, [&](const uint8_t* b, size_t n) {
            if (file.write(b, n) != n) {
                writeFailed = true;
                return false;
            }
            return true;
        }, progress, r);
        if (writeFailed) {
            r.status = -107;
            r.error = "SD-Karte voll oder schreibgeschuetzt";
        }
    } else {
        fillError(r, http);
    }
    http.end();
    return r;
}

Result getText(const String& url, const String& bearer, String& out, size_t maxBytes, bool& truncated) {
    truncated = false;
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    if (!open(http, client, url, bearer)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    r.status = http.GET();
    if (r.ok()) {
        out = "";
        out.reserve(min((int)maxBytes, max(0, http.getSize())) + 1);
        readBody(http, [&](const uint8_t* b, size_t n) {
            size_t room = maxBytes - out.length();
            out.concat((const char*)b, min(n, room));
            if (n > room) {
                truncated = true;
                return false;
            }
            return true;
        }, nullptr, r);
    } else {
        fillError(r, http);
    }
    http.end();
    return r;
}

// Stream aus drei Teilen: Kopf (Speicher) + Datei (SD) + Schluss (Speicher)
class MultipartStream : public Stream {
public:
    MultipartStream(const String& head, fs::File& file, const String& tail, const Progress& p)
        : _head(head), _file(file), _tail(tail), _progress(p), _total(head.length() + file.size() + tail.length()) {}

    size_t total() const { return _total; }
    int available() override { return _total - _pos; }
    int read() override {
        uint8_t c;
        return readBytes((char*)&c, 1) == 1 ? c : -1;
    }
    int peek() override { return -1; }
    size_t write(uint8_t) override { return 0; }
    void flush() override {}

    size_t readBytes(char* buf, size_t len) override {
        size_t n = 0;
        while (n < len && _pos < _total) {
            size_t fileEnd = _head.length() + _file.size();
            size_t k;
            if (_pos < _head.length()) {
                k = min(len - n, _head.length() - _pos);
                memcpy(buf + n, _head.c_str() + _pos, k);
            } else if (_pos < fileEnd) {
                k = _file.read((uint8_t*)buf + n, min(len - n, fileEnd - _pos));
                if (k == 0) break;  // Lesefehler
            } else {
                k = min(len - n, _total - _pos);
                memcpy(buf + n, _tail.c_str() + (_pos - fileEnd), k);
            }
            n += k;
            _pos += k;
        }
        if (_progress && millis() - _lastDraw > 200) {
            _progress(_pos, _total);
            _lastDraw = millis();
        }
        return n;
    }

private:
    const String& _head;
    fs::File& _file;
    const String& _tail;
    Progress _progress;
    size_t _total;
    size_t _pos = 0;
    uint32_t _lastDraw = 0;
};

static String quoteHeader(const String& s) {
    String out;
    for (char c : s) out += (c == '"' || c == '\r' || c == '\n') ? '_' : c;
    return out;
}

Result uploadFile(const String& url, const String& bearer, const char* field, fs::File& file,
                  const String& fileName, const String& extraName, const String& extraValue,
                  JsonDocument& out, const Progress& progress) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    if (!open(http, client, url, bearer)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    String boundary = "----raveneye" + String(esp_random(), HEX) + String(esp_random(), HEX);
    String head;
    if (!extraName.isEmpty()) {
        head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + extraName + "\"\r\n\r\n" +
                extraValue + "\r\n";
    }
    head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + field + "\"; filename=\"" +
            quoteHeader(fileName) + "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    MultipartStream body(head, file, tail, progress);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    r.status = http.sendRequest("POST", &body, body.total());
    if (progress) progress(body.total(), body.total());
    if (r.ok()) deserializeJson(out, http.getStream());
    else fillError(r, http);
    http.end();
    return r;
}

}  // namespace net
