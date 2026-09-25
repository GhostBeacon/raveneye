// Anmeldung am eigenen Server (<server_url>/api/v1) fuer die Systemwerte.
// Das Geraet fordert ein Nur-Lese-Token an (scope=monitor): Es darf nur Systemwerte lesen,
// keine Dateien. Passwort und 2FA-Code werden nicht gespeichert, nur das Token (NVS).
#pragma once

#include <Arduino.h>

namespace auth {
void begin();          // Token aus dem NVS laden
bool loggedIn();
String token();
String user();
void login();          // Dialog: Benutzername, Passwort, ggf. 2FA-Code
void logout();         // Token auch auf dem Server loeschen
void forget();         // nur lokal loeschen (z. B. nach 401)
}
