# RavenEye 🐦‍⬛

Eigene Firmware für den **M5Stack Cardputer ADV** (ESP32-S3FN8, 8 MB Flash, Tastatur-Chip TCA8418), gestartet über den [M5Launcher](https://github.com/bmorcelli/Launcher). Ein kleines Auge, das wach bleibt: Es empfängt Push-Meldungen und schlägt Alarm, wenn etwas passiert.

Ein Startmenü mit vier Funktionen. WLAN und ntfy laufen immer im Hintergrund weiter, auch wenn gerade eine andere Funktion offen ist – neue Meldungen erscheinen dann als Hinweis unten am Bildschirm.

| # | Funktion | Was sie tut |
|---|---|---|
| 1 | **Meldungen** | [ntfy](https://ntfy.sh)-Empfänger für ein oder mehrere Topics |
| 2 | **Status** | Dienste einer [Uptime-Kuma](https://github.com/louislam/uptime-kuma)-Statusseite, mit Verlauf |
| 3 | **Dateien** | Client für einen eigenen Dateimanager mit JSON-API (`/api/v1`): durchsuchen, herunter- und hochladen |
| 4 | **Einstellungen** | Ton, Helligkeit, Abdunkeln, WLAN, SD-Karte, Abmelden, Neustart |

## Bedienung

| Taste | Wirkung |
|---|---|
| `;` `.` | hoch / runter (gedrückt halten = weiter) |
| `,` `/` | seitenweise zurück / vor |
| Enter | öffnen / auswählen |
| `` ` `` (Esc) oder Del | zurück – aus einer Funktion ins Menü |
| `1`–`4` | im Menü: Funktion direkt öffnen |
| `m` | im Menü und bei den Meldungen: Ton an/aus (bleibt gespeichert) |

**Texteingabe** (Anmeldung, neuer Ordner): Enter = OK, Del = Zeichen löschen, **Fn + `** = abbrechen, Tab = Passwort zeigen/verbergen.

Oben rechts: Punkt grün = ntfy-Stream läuft, gelb = WLAN ohne Stream, rot = kein WLAN; `• 3` = ungelesene Meldungen; dazu Uhrzeit und Akku. Die Version steht auf dem Startbildschirm (`1.0.<Build-Nr.>`, lokal gebaut: `dev`).

### Meldungen

- Beim Start werden die Meldungen der letzten 12 h geladen (Cache von ntfy.sh), danach kommen neue live. Nach einem Verbindungsabbruch holt `since=<letzte id>` Verpasstes nach.
- Ton bei neuen Meldungen, doppelt ab Priorität 4. Farbe nach Priorität (5 rot, 4 orange, 1–2 grau).
- Enter öffnet die Meldung, `x` entfernt sie vom Gerät, `a` markiert alle als gelesen.

### Status

- Liest die öffentliche Statusseite von Uptime Kuma 1.x (`/api/status-page/<slug>` und `/api/status-page/heartbeat/<slug>`), jede Minute neu, solange die Funktion offen ist; `r` lädt sofort.
- Pro Dienst: Status (grün läuft, rot ausgefallen, orange ausstehend, blau Wartung) und Verfügbarkeit der letzten 24 h. Enter zeigt die letzten 50 Messungen als Balken und die Antwortzeit.
- Voraussetzung: In Uptime Kuma eine Statusseite anlegen und die gewünschten Monitore hinzufügen. **Die Seite ist öffentlich** – wer die Adresse kennt, sieht die Namen der Monitore.

### Dateien

- Anmeldung auf dem Gerät mit Benutzername, Passwort und – falls aktiv – 2FA-Code. Das Gerät erhält ein eigenes Token (Gerätename „RavenEye“); Passwort und Code werden nicht gespeichert.
- Enter öffnet Ordner; bei Dateien: **auf SD-Karte laden** (nach `/raveneye/downloads`), **anzeigen** (Textdateien bis 16 KB) oder **in den Papierkorb**.
- `u` lädt eine Datei von der SD-Karte in den aktuellen Ordner hoch (gestreamt, auch große Dateien), `n` legt einen Ordner an, `x` verschiebt in den Papierkorb, `i` zeigt Konto und Speicher, `r` lädt neu.

### Schrift

Adobe Helvetica 10 (normal/fett) mit Umlauten, ß, €, °, „…“, – und …. Emojis werden weggelassen, andere unbekannte Zeichen als `?` angezeigt.

## Konfiguration

Die Firmware enthält keine Zugangsdaten. Sie liest beim Start `/raveneye/config.txt` von der SD-Karte, Vorlage: [`sd-beispiel/raveneye/config.txt`](sd-beispiel/raveneye/config.txt).

```
wifi_ssid=<WLAN_NAME>
wifi_pass=<WLAN_PASSWORT>
wifi2_ssid=<WLAN2_NAME>        # optional, bis wifi9_*
wifi2_pass=<WLAN2_PASSWORT>
ntfy_server=ntfy.sh
ntfy_topics=<TOPIC>[,<TOPIC2>]
status_url=https://<STATUS_HOST>   # optional
status_slug=<SLUG>
cloud_url=https://<CLOUD_HOST>     # optional
```

- Pro Zeile `name=wert`. Zeilen, die mit `#` beginnen, sind Kommentare. Leerzeichen am Rand des Werts werden entfernt.
- Bis zu 9 WLANs (`wifi_*`, `wifi2_*` … `wifi9_*`). Verbunden wird mit dem stärksten, das gerade erreichbar ist; bei Abbruch wird alle 10 s neu gesucht.
- Die WLANs müssen 2,4 GHz können, der ESP32-S3 kann kein 5 GHz.
- Topics: nur `A–Z a–z 0–9 - _`, mehrere mit Komma ohne Leerzeichen.
- `status_url` und `cloud_url`: nur `https://`, ohne Pfad am Ende. Fehlen sie, zeigt die jeweilige Funktion „nicht eingerichtet“.

| Fehleranzeige | Ursache |
|---|---|
| `Keine SD-Karte` | Karte fehlt oder ist nicht FAT32 |
| `/raveneye/config.txt fehlt` | Ordner oder Dateiname falsch |
| `wifi_ssid fehlt` | kein einziges WLAN eingetragen oder Schlüssel falsch geschrieben |
| `ntfy_topics ungueltig` | Leerzeichen, spitze Klammern oder Sonderzeichen im Topic |
| `URL muss https:// sein` | `status_url` oder `cloud_url` beginnt nicht mit `https://` |

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
- **Datei-Token im internen Speicher (NVS)**, nicht auf der SD-Karte – aber unverschlüsselt. Geht das Gerät verloren, das Token auf dem Server widerrufen (Gerät „RavenEye“). Abmelden in den Einstellungen löscht es auch auf dem Server.
- Alle Verbindungen (ntfy, Status, Dateien) laufen über HTTPS mit Prüfung gegen dieselben Stammzertifikate. Server mit Zertifikaten anderer Anbieter als Let's Encrypt werden abgelehnt.

## Aufbau

```
├── platformio.ini
├── .github/workflows/firmware.yml   # Bauen + Release
├── sd-beispiel/raveneye/config.txt  # Vorlage mit Platzhaltern
└── src/
    ├── main.cpp          # Start, Hauptschleife, Wechsel zwischen den Funktionen
    ├── core.cpp/.h       # Hintergrund: WLAN, ntfy, Meldungen, Einstellungen, Abdunkeln
    ├── keys.cpp/.h       # Tastatur mit Wiederholung
    ├── ui.cpp/.h         # Kopfzeile, Listen, Dialoge, Texteingabe, Fortschritt
    ├── text.cpp/.h       # UTF-8, Zeichenvorrat, Zahlen/Zeiten
    ├── net.cpp/.h        # HTTPS: JSON, Download, Upload (gestreamt)
    ├── app_home.cpp      # Startmenü
    ├── app_ntfy.cpp      # Meldungen
    ├── app_status.cpp    # Uptime Kuma
    ├── app_files.cpp     # Dateien
    ├── app_settings.cpp  # Einstellungen
    ├── ntfy.cpp/.h       # ntfy-Stream-Client
    ├── config.cpp/.h     # SD-Konfiguration
    ├── certs.h           # Root-Zertifikate
    └── fonts.c/.h        # Schriften (u8g2-Format, Lizenz im Dateikopf)
```

## Offen

- [ ] Mehr Dateiaktionen (umbenennen, verschieben, Papierkorb wiederherstellen)
- [ ] Statusanzeige auch im Hintergrund prüfen und bei Ausfall melden
