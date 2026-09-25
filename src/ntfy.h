// ntfy-Abo ueber den JSON-Stream (GET /<topics>/json, HTTP/1.0, eine Zeile pro Ereignis).
// Nicht blockierend: poll() in jedem loop() aufrufen. Nur der Verbindungsaufbau blockiert kurz.
#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include <functional>

struct NtfyMessage {
    String id;
    String topic;
    String title;
    String message;
    uint32_t time = 0;
    uint8_t priority = 3;  // 1..5
    bool seen = false;
};

class NtfyClient {
public:
    enum class State { Idle, Streaming, Backoff };

    void begin(const String& server, const String& topics, std::function<void(NtfyMessage&&)> onMessage);
    void poll(bool networkReady);
    State state() const { return _state; }

private:
    bool connect();
    void handleLine(const String& line);
    void disconnect();

    WiFiClientSecure _client;
    String _server;
    String _topics;
    String _since = "12h";  // beim Start: alles, was ntfy.sh noch vorhaelt (12 h Cache)
    String _buf;
    State _state = State::Idle;
    uint32_t _lastData = 0;
    uint32_t _retryAt = 0;
    uint32_t _backoff = 5000;
    std::function<void(NtfyMessage&&)> _onMessage;
};
