#include "ntfy.h"

#include <ArduinoJson.h>

#include "certs.h"

// ntfy schickt alle ~45 s ein keepalive; ohne Daten so lange gilt die Verbindung als tot.
static constexpr uint32_t STALL_TIMEOUT_MS = 120000;
static constexpr uint32_t BACKOFF_MAX_MS = 60000;
static constexpr size_t MAX_LINE = 8192;

void NtfyClient::begin(const String& server, const String& topics, std::function<void(NtfyMessage&&)> onMessage) {
    _server = server;
    _topics = topics;
    _onMessage = std::move(onMessage);
    _client.setCACert(ROOT_CAS);
}

bool NtfyClient::connect() {
    _client.setTimeout(10);  // Sekunden
    if (!_client.connect(_server.c_str(), 443)) return false;

    _client.printf("GET /%s/json?since=%s HTTP/1.0\r\n"
                   "Host: %s\r\n"
                   "User-Agent: raveneye\r\n"
                   "\r\n",
                   _topics.c_str(), _since.c_str(), _server.c_str());

    String status = _client.readStringUntil('\n');
    if (status.indexOf(" 200 ") < 0) {
        Serial.printf("ntfy: %s\n", status.c_str());
        _client.stop();
        return false;
    }
    // Header ueberspringen bis zur Leerzeile
    while (_client.connected()) {
        String h = _client.readStringUntil('\n');
        if (h == "\r" || h.isEmpty()) break;
    }
    _buf = "";
    _lastData = millis();
    return true;
}

void NtfyClient::disconnect() {
    _client.stop();
    _state = State::Backoff;
    _retryAt = millis() + _backoff;
    _backoff = min(_backoff * 2, BACKOFF_MAX_MS);
}

void NtfyClient::poll(bool networkReady) {
    if (!networkReady) {
        if (_state == State::Streaming) _client.stop();
        _state = State::Idle;
        return;
    }

    if (_state == State::Idle || (_state == State::Backoff && (int32_t)(millis() - _retryAt) >= 0)) {
        if (connect()) {
            _state = State::Streaming;
            _backoff = 5000;
        } else {
            disconnect();
        }
        return;
    }

    if (_state != State::Streaming) return;

    while (_client.available()) {
        char c = _client.read();
        _lastData = millis();
        if (c == '\n') {
            handleLine(_buf);
            _buf = "";
        } else if (_buf.length() < MAX_LINE) {
            _buf += c;
        }
    }

    if (!_client.connected() || millis() - _lastData > STALL_TIMEOUT_MS) disconnect();
}

void NtfyClient::handleLine(const String& line) {
    JsonDocument doc;
    if (deserializeJson(doc, line)) return;
    if (strcmp(doc["event"] | "", "message") != 0) return;  // open / keepalive ignorieren

    NtfyMessage m;
    m.id = doc["id"] | "";
    m.topic = doc["topic"] | "";
    m.title = doc["title"] | "";
    m.message = doc["message"] | "";
    m.time = doc["time"] | 0;
    m.priority = doc["priority"] | 3;
    if (m.id.isEmpty()) return;

    _since = m.id;  // nach Neuverbindung nur Neues holen
    if (_onMessage) _onMessage(std::move(m));
}
