// HTTPS-Anfragen mit Zertifikatspruefung (certs.h). Blockierend, mit Zeitlimit.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>

#include <functional>

namespace net {

using Progress = std::function<void(uint64_t done, uint64_t total)>;

struct Result {
    int status = 0;   // HTTP-Status, <= 0 = Verbindungsfehler
    String error;     // lesbare Meldung, leer bei Erfolg (2xx)
    String code;      // "error"-Feld der API (z. B. "totp_required")
    bool ok() const { return status >= 200 && status < 300; }
};

// GET und JSON lesen. filter (optional) spart Speicher bei grossen Antworten.
Result getJson(const String& url, const String& bearer, JsonDocument& out, const JsonDocument* filter = nullptr);

// POST mit JSON-Koerper; Antwort (falls JSON) landet in out.
Result postJson(const String& url, const String& bearer, const JsonDocument& body, JsonDocument& out);

// GET direkt in eine Datei (SD). Bricht nach 15 s ohne Daten ab.
Result download(const String& url, const String& bearer, fs::File& file, const Progress& progress);

// GET in einen String, hoechstens maxBytes (Rest wird verworfen, truncated = true).
Result getText(const String& url, const String& bearer, String& out, size_t maxBytes, bool& truncated);

// multipart/form-data-Upload einer Datei, gestreamt von der SD-Karte.
Result uploadFile(const String& url, const String& bearer, const char* field, fs::File& file,
                  const String& fileName, const String& extraName, const String& extraValue,
                  JsonDocument& out, const Progress& progress);

}  // namespace net
