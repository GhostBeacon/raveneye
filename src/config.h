// Konfiguration von der SD-Karte: /raveneye/config.txt (key=value, # = Kommentar).
// Zugangsdaten liegen nur auf der Karte, nie in der Firmware oder im Repo.
#pragma once

#include <Arduino.h>

struct Config {
    String wifiSsid;
    String wifiPass;
    String ntfyServer = "ntfy.sh";  // nur Hostname, immer HTTPS
    String ntfyTopics;              // kommagetrennt, z. B. "alarm,info"
};

// Liefert false und eine Fehlermeldung, wenn Karte/Datei fehlt oder Pflichtwerte leer sind.
bool loadConfig(Config& cfg, String& error);
