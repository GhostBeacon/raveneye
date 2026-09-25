#include "net.h"

#include "certs.h"
#include "core.h"

namespace net {

static constexpr uint16_t TIMEOUT_MS = 15000;

static void setup(HTTPClient& http, WiFiClientSecure& client) {
    client.setCACert(ROOT_CAS);
    client.setTimeout(TIMEOUT_MS / 1000);
    http.setTimeout(TIMEOUT_MS);
    http.setConnectTimeout(TIMEOUT_MS);
    http.setUserAgent("raveneye");
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

// Antwort lesen: bei Erfolg und bei Fehlern (API-Fehler sind JSON: {"error", "message", ...}).
// stream = direkt aus der Verbindung parsen (nur mit HTTP/1.0, spart bei grossen Antworten
// den Zwischenpuffer); sonst erst als String lesen (auch chunked).
static void readResponse(Result& r, HTTPClient& http, JsonDocument& out, const JsonDocument* filter, bool stream) {
    if (r.status <= 0) {
        r.error = "Keine Verbindung (" + HTTPClient::errorToString(r.status) + ")";
        return;
    }
    if (stream && r.ok()) {
        DeserializationError e = filter ? deserializeJson(out, http.getStream(), DeserializationOption::Filter(*filter))
                                        : deserializeJson(out, http.getStream());
        if (e && e != DeserializationError::EmptyInput) {
            r.status = -104;
            r.error = String("Antwort unlesbar (") + e.c_str() + ")";
        }
        return;
    }
    String body = http.getString();
    DeserializationError e = body.isEmpty() ? DeserializationError::EmptyInput
                             : filter        ? deserializeJson(out, body, DeserializationOption::Filter(*filter))
                                             : deserializeJson(out, body);
    if (r.ok()) {
        if (e && !body.isEmpty()) {
            r.status = -104;
            r.error = String("Antwort unlesbar (") + e.c_str() + ")";
        }
        return;
    }
    if (!e) {
        r.code = out["error"] | "";
        r.error = out["message"] | "";
    }
    if (r.error.isEmpty()) r.error = "Serverfehler " + String(r.status);
}

Result getJson(const String& url, const String& bearer, JsonDocument& out, const JsonDocument* filter) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    setup(http, client);
    http.useHTTP10(true);
    if (!http.begin(client, url)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    if (!bearer.isEmpty()) http.addHeader("Authorization", "Bearer " + bearer);
    r.status = http.GET();
    readResponse(r, http, out, filter, true);
    http.end();
    return r;
}

Result postJson(const String& url, const String& bearer, const JsonDocument& body, JsonDocument& out) {
    Result r = precheck(url);
    if (r.status < 0) return r;
    WiFiClientSecure client;
    HTTPClient http;
    setup(http, client);
    http.useHTTP10(true);
    if (!http.begin(client, url)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    if (!bearer.isEmpty()) http.addHeader("Authorization", "Bearer " + bearer);
    http.addHeader("Content-Type", "application/json");
    String payload;
    serializeJson(body, payload);
    r.status = http.POST(payload);
    for (unsigned i = 0; i < payload.length(); i++) payload.setCharAt(i, 0);  // Passwort nicht im RAM lassen
    readResponse(r, http, out, nullptr, false);
    http.end();
    return r;
}

Result Session::getJson(const String& url, const String& bearer, JsonDocument& out) {
    Result r = precheck(url);
    if (r.status < 0) {
        close();
        return r;
    }
    if (!_client) {
        _client = new WiFiClientSecure();
        _http = new HTTPClient();
        setup(*_http, *_client);
        _http->setReuse(true);  // HTTP/1.1 keep-alive
    }
    if (!_http->begin(*_client, url)) {
        r.status = -103;
        r.error = "Adresse ungueltig";
        return r;
    }
    if (!bearer.isEmpty()) _http->addHeader("Authorization", "Bearer " + bearer);
    r.status = _http->GET();
    readResponse(r, *_http, out, nullptr, false);
    _http->end();                // haelt bei setReuse(true) die Verbindung offen
    if (r.status <= 0) close();  // Verbindungsfehler: beim naechsten Mal frisch verbinden
    return r;
}

void Session::close() {
    if (_http) {
        _http->end();
        delete _http;
        _http = nullptr;
    }
    if (_client) {
        _client->stop();
        delete _client;
        _client = nullptr;
    }
}

}  // namespace net
