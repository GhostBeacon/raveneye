# RavenEye 🐦‍⬛

Eigene Firmware für den **M5Stack Cardputer ADV** (ESP32-S3FN8, 8 MB Flash, Tastatur-Chip TCA8418), gestartet über den [M5Launcher](https://github.com/bmorcelli/Launcher). Ein kleines Auge, das wach bleibt: Es empfängt Push-Meldungen und schlägt Alarm, wenn etwas passiert.

**Aktuell:** ntfy-Empfänger, zum Beispiel für Alarme eigener Server und Skripte.
**Geplant:** Statusanzeige, Dateimanager-Client, Menü zum Wechseln zwischen den Funktionen.

## Funktionen

- Abonniert ein oder mehrere [ntfy](https://ntfy.sh)-Topics über den JSON-Stream (`GET /<topics>/json`, HTTP/1.0).
- Beim Start werden die Meldungen der letzten 12 h geladen (Cache von ntfy.sh), danach kommen neue Meldungen live. Nach einem Verbindungsabbruch holt `since=<letzte id>` Verpasstes nach. Neuverbindung mit Backoff 5 s → 60 s.
- Ton bei neuen Meldungen, doppelt ab Priorität 4. Farbe nach Priorität (5 rot, 4 orange, 1–2 grau).
- Display wird nach 60 s ohne Taste gedimmt. Eine neue Meldung oder eine Taste weckt es.
- Status oben rechts: Punkt grün = Stream läuft, gelb = WLAN ohne Stream, rot = kein WLAN. Dazu Uhrzeit und Akku.
- Schrift: Adobe Helvetica 10 (normal/fett) mit Umlauten, ß, €, °, „…“, – und …. Emojis werden weggelassen, andere unbekannte Zeichen als `?` angezeigt.
- Die Version steht auf dem Startbildschirm (`1.0.<Build-Nr.>`, lokal gebaut: `dev`).

| Taste | Liste | Meldung |
|---|---|---|
| `;` / `.` | hoch / runter | scrollen |
| Enter | Meldung öffnen | – |
| `` ` `` (Esc) / Del | – | zurück |
| `m` | Ton an/aus | Ton an/aus |

## Konfiguration

Die Firmware enthält keine Zugangsdaten. Sie liest beim Start `/raveneye/config.txt` von der SD-Karte, Vorlage: [`sd-beispiel/raveneye/config.txt`](sd-beispiel/raveneye/config.txt).

```
wifi_ssid=<WLAN_NAME>
wifi_pass=<WLAN_PASSWORT>
wifi2_ssid=<WLAN2_NAME>        # optional, bis wifi9_*
wifi2_pass=<WLAN2_PASSWORT>
ntfy_server=ntfy.sh
ntfy_topics=<TOPIC>[,<TOPIC2>]
```

- Pro Zeile `name=wert`. Zeilen, die mit `#` beginnen, sind Kommentare. Leerzeichen am Rand des Werts werden entfernt.
- Bis zu 9 WLANs (`wifi_*`, `wifi2_*` … `wifi9_*`). Verbunden wird mit dem stärksten, das gerade erreichbar ist; bei Abbruch wird alle 10 s neu gesucht.
- Die WLANs müssen 2,4 GHz können, der ESP32-S3 kann kein 5 GHz.
- Topics: nur `A–Z a–z 0–9 - _`, mehrere mit Komma ohne Leerzeichen.

| Fehleranzeige | Ursache |
|---|---|
| `Keine SD-Karte` | Karte fehlt oder ist nicht FAT32 |
| `/raveneye/config.txt fehlt` | Ordner oder Dateiname falsch |
| `wifi_ssid fehlt` | kein einziges WLAN eingetragen oder Schlüssel falsch geschrieben |
| `ntfy_topics ungueltig` | Leerzeichen, spitze Klammern oder Sonderzeichen im Topic |

## Installieren

**Über WLAN mit dem M5Launcher:** Bei jedem Push auf `main` baut GitHub Actions die Firmware und legt ein Release an. Der Link auf die neueste Version bleibt immer gleich:

```
https://github.com/GhostBeacon/raveneye/releases/latest/download/raveneye.bin
```

Einmalig als Favorit eintragen: SD-Karte in den Rechner, `/config.conf` vorher sichern, dann in der Liste `"favorite": [ … ]` ergänzen (vom vorigen Eintrag mit Komma trennen):

```json
{
  "name": "RavenEye",
  "fid": "",
  "link": "https://github.com/GhostBeacon/raveneye/releases/latest/download/raveneye.bin"
}
```

`fid` muss leer bleiben, sonst sucht der Launcher im M5Burner-Katalog statt über den Link. Danach im Launcher: **OTA → Favorite List → RavenEye → Install**, künftige Updates genauso.

**Von der SD-Karte:** `raveneye.bin` aus einem [Release](https://github.com/GhostBeacon/raveneye/releases) (oder eine lokal gebaute `firmware.bin`) auf die Karte kopieren, im Launcher **SD** → Datei → **Install**.

Zum Launcher zurück: beim Einschalten eine Taste drücken, solange der Launcher-Startbildschirm zu sehen ist.

## Bauen

```bash
python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/pip install platformio
~/.platformio/penv/bin/pio run
```

Ergebnis: `.pio/build/cardputer-adv/firmware.bin`. Serielle Ausgabe über USB: `~/.platformio/penv/bin/pio device monitor`.

## Sicherheit

- **TLS mit Zertifikatsprüfung.** In [`src/certs.h`](src/certs.h) stehen nur ISRG Root X1 und X2 (Let's Encrypt), mit SHA-256-Fingerabdruck. Verbunden wird erst nach erfolgreichem NTP-Abgleich, denn ohne gültige Uhrzeit schlägt die Prüfung fehl.
- **Zugangsdaten nur auf der SD-Karte**, nie in Firmware oder Repo. Wer die Karte hat, hat WLAN-Passwort und Topic.

## Aufbau

```
├── platformio.ini
├── .github/workflows/firmware.yml   # Bauen + Release
├── sd-beispiel/raveneye/config.txt   # Vorlage mit Platzhaltern
└── src/
    ├── main.cpp      # Anzeige, Tasten, Ablauf
    ├── ntfy.cpp/.h   # Stream-Client mit Neuverbindung
    ├── config.cpp/.h # SD-Konfiguration
    ├── certs.h       # Root-Zertifikate
    └── fonts.c/.h    # Schriften (u8g2-Format, Lizenz im Dateikopf)
```

## Offen

- [ ] Ton bei neuen Meldungen prüfen (kam beim ersten Test nicht)
- [ ] Statusanzeige für eigene Dienste
- [ ] Dateimanager-Client
- [ ] Menü zum Wechseln zwischen den Funktionen
