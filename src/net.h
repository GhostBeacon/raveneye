// HTTPS-Anfragen mit Zertifikatspruefung (certs.h). Blockierend, mit Zeitlimit.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace net {

struct Result {
    int status = 0;   // HTTP-Status, <= 0 = Verbindungsfehler
    String error;     // lesbare Meldung, leer bei Erfolg (2xx)
    String code;      // "error"-Feld der API (z. B. "totp_required")
    bool ok() const { return status >= 200 && status < 300; }
};

// GET und JSON lesen. filter (optional) spart Speicher bei grossen Antworten.
Result getJson(const String& url, const String& bearer, JsonDocument& out, const JsonDocument* filter = nullptr);

// POST mit JSON-Koerper. Die Antwort (auch eine Fehlerantwort) landet in out.
Result postJson(const String& url, const String& bearer, const JsonDocument& body, JsonDocument& out);

// Dauerhafte Verbindung fuer regelmaessige Abfragen desselben Servers (spart den
// TLS-Handshake bei jeder Abfrage). close() gibt den Speicher frei.
class Session {
public:
    Result getJson(const String& url, const String& bearer, JsonDocument& out);
    void close();
    ~Session() { close(); }

private:
    WiFiClientSecure* _client = nullptr;
    HTTPClient* _http = nullptr;
};

}  // namespace net
