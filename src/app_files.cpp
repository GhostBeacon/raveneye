// Dateimanager-Client fuer eine JSON-API unter <cloud_url>/api/v1 (Bearer-Token):
//   POST /login {username, password, totp_code?, device_name} -> {token, user}
//        401 totp_required -> 2FA-Code abfragen und erneut senden
//   GET  /me, /list?folder_id=    POST /folders, /delete, /logout, /upload (multipart, Feld "file")
//   GET  /files/<id>/download
// Das Token liegt im NVS des Geraets (nicht auf der SD-Karte). Passwort und 2FA-Code werden
// nicht gespeichert. Verlorenes Geraet: Token auf dem Server widerrufen.

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <vector>

#include "apps.h"
#include "core.h"
#include "net.h"
#include "text.h"
#include "ui.h"

static const char* DOWNLOAD_DIR = "/raveneye/downloads";
static constexpr size_t TEXT_PREVIEW_MAX = 16 * 1024;

struct Entry {
    bool folder = false;
    int id = 0;
    String name;
    double sizeMb = 0;
    String ext;
};

static String token, user;
static int folderId = 0;     // 0 = Wurzel
static int parentId = 0;
static String folderPath;    // Brotkrumen, z. B. "Fotos / 2026"
static std::vector<Entry> entries;
static bool loaded = false;
static String lastError;
static int sel = 0, top = 0;

static String api(const String& path) { return core::cfg.cloudUrl + "/api/v1" + path; }
static bool configured() { return !core::cfg.cloudUrl.isEmpty(); }
static bool loggedIn() { return !token.isEmpty(); }

void filesBegin() {
    token = core::prefs.getString("cloud_token", "");
    user = core::prefs.getString("cloud_user", "");
}

static void forgetLogin() {
    token = "";
    user = "";
    core::prefs.remove("cloud_token");
    core::prefs.remove("cloud_user");
    entries.clear();
    loaded = false;
}

// 401 bei einer Anfrage mit Token = Sitzung abgelaufen oder widerrufen
static bool handleAuth(const net::Result& r) {
    if (r.status != 401) return false;
    forgetLogin();
    ui::message("Abgemeldet", r.error.isEmpty() ? "Sitzung ungültig - bitte neu anmelden." : r.error, TFT_ORANGE);
    return true;
}

// ---------- Anmeldung ----------

static void login() {
    String name = user, pass, code;
    if (!ui::textInput("Benutzername", name, false, 64) || name.isEmpty()) return;
    if (!ui::textInput("Passwort", pass, true, 128) || pass.isEmpty()) return;

    while (true) {
        ui::busy("Anmelden", "Verbinde ...");
        JsonDocument body, resp;
        body["username"] = name;
        body["password"] = pass;
        body["device_name"] = "RavenEye";
        if (!code.isEmpty()) body["totp_code"] = code;
        net::Result r = net::postJson(api("/login"), "", body, resp);

        if (r.ok()) {
            token = resp["token"] | "";
            user = resp["user"]["username"] | name.c_str();
            break;
        }
        if (r.code == "totp_required" || r.code == "invalid_totp") {
            if (r.code == "invalid_totp") ui::message("Anmelden", r.error, TFT_ORANGE);
            code = "";
            if (!ui::textInput("2FA-Code", code, false, 8) || code.isEmpty()) break;
            continue;
        }
        String msg = r.error;
        if (!resp["remaining_attempts"].isNull()) msg += "\nNoch " + String((int)resp["remaining_attempts"]) + " Versuche.";
        ui::message("Anmelden fehlgeschlagen", msg, TFT_RED);
        break;
    }
    for (unsigned i = 0; i < pass.length(); i++) pass.setCharAt(i, 0);

    if (!token.isEmpty()) {
        core::prefs.putString("cloud_token", token);
        core::prefs.putString("cloud_user", user);
        folderId = parentId = 0;
        loaded = false;
    }
}

// ---------- Ordner laden ----------

static void load(int id) {
    ui::busy("Dateien", "Lade Ordner ...");
    JsonDocument filter;
    filter["folder"]["parent_id"] = true;
    filter["breadcrumb"][0]["name"] = true;
    filter["folders"][0]["id"] = true;
    filter["folders"][0]["name"] = true;
    filter["folders"][0]["size_mb"] = true;
    filter["files"][0]["id"] = true;
    filter["files"][0]["name"] = true;
    filter["files"][0]["size_mb"] = true;
    filter["files"][0]["ext"] = true;
    JsonDocument doc;
    String url = api("/list") + (id ? "?folder_id=" + String(id) : String());
    net::Result r = net::getJson(url, token, doc, &filter);
    if (handleAuth(r)) return;
    if (!r.ok()) {
        lastError = r.error;
        if (r.status == 404 && id) {  // Ordner verschwunden -> Wurzel
            folderId = parentId = 0;
            folderPath = "";
        }
        return;
    }
    lastError = "";
    folderId = id;
    parentId = doc["folder"]["parent_id"] | 0;
    folderPath = "";
    for (JsonObject b : doc["breadcrumb"].as<JsonArray>()) {
        if (!folderPath.isEmpty()) folderPath += " / ";
        folderPath += text::sanitize(b["name"] | "");
    }
    entries.clear();
    auto add = [](JsonObject f, bool folder) {
        Entry e;
        e.folder = folder;
        e.id = f["id"] | 0;
        e.name = text::sanitize(f["name"] | "");
        e.sizeMb = f["size_mb"] | 0.0;
        e.ext = f["ext"] | "";
        entries.push_back(e);
    };
    for (JsonObject f : doc["folders"].as<JsonArray>()) add(f, true);
    for (JsonObject f : doc["files"].as<JsonArray>()) add(f, false);
    loaded = true;
    sel = top = 0;
}

// ---------- Aktionen ----------

static String fatName(const String& name) {
    String out;
    for (char c : name) out += strchr("\\/:*?\"<>|", c) ? '_' : c;
    out.trim();
    return out.isEmpty() ? String("datei") : out;
}

static uint64_t sdFree() { return SD.totalBytes() - SD.usedBytes(); }

static void download(const Entry& e) {
    uint64_t need = (uint64_t)(e.sizeMb * 1024 * 1024);
    if (need + 1024 * 1024 > sdFree()) {
        ui::message("Herunterladen", "Nicht genug Platz auf der SD-Karte (frei: " + text::fmtBytes(sdFree()) + ").", TFT_RED);
        return;
    }
    SD.mkdir("/raveneye");
    SD.mkdir(DOWNLOAD_DIR);
    String base = fatName(e.name), path = String(DOWNLOAD_DIR) + "/" + base;
    for (int n = 2; SD.exists(path); n++) {  // nicht ueberschreiben
        int dot = base.lastIndexOf('.');
        String stem = dot > 0 ? base.substring(0, dot) : base, ext = dot > 0 ? base.substring(dot) : "";
        path = String(DOWNLOAD_DIR) + "/" + stem + " (" + n + ")" + ext;
    }
    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        ui::message("Herunterladen", "Datei auf der SD-Karte nicht anlegbar.", TFT_RED);
        return;
    }
    net::Result r = net::download(api("/files/" + String(e.id) + "/download"), token, f,
                                  [&](uint64_t done, uint64_t total) {
                                      ui::progress("Herunterladen", e.name, done, total ? total : need);
                                  });
    f.close();
    if (handleAuth(r)) {
        SD.remove(path);
        return;
    }
    if (!r.ok()) {
        SD.remove(path);  // keine halben Dateien liegen lassen
        ui::message("Herunterladen fehlgeschlagen", r.error, TFT_RED);
        return;
    }
    ui::message("Heruntergeladen", "Gespeichert unter\n" + path, TFT_GREEN);
}

static bool isText(const Entry& e) {
    static const char* exts[] = {"txt", "md", "log", "csv", "json", "conf", "cfg", "ini", "yml", "yaml", "xml", "sh", "py"};
    for (auto x : exts)
        if (e.ext == x) return true;
    return false;
}

static void preview(const Entry& e) {
    ui::busy("Anzeigen", "Lade " + e.name + " ...");
    String body;
    bool truncated = false;
    net::Result r = net::getText(api("/files/" + String(e.id) + "/download"), token, body, TEXT_PREVIEW_MAX, truncated);
    if (handleAuth(r)) return;
    if (!r.ok()) {
        ui::message("Anzeigen fehlgeschlagen", r.error, TFT_RED);
        return;
    }
    body = text::sanitize(body);
    if (truncated) body += "\n\n[... gekürzt, nur die ersten 16 KB]";
    ui::viewText(e.name, body);
}

static void trash(const Entry& e) {
    if (!ui::confirm("Papierkorb", "\"" + e.name + "\" in den Papierkorb verschieben?")) return;
    ui::busy("Papierkorb", "Verschiebe ...");
    JsonDocument body, resp;
    body[e.folder ? "folder_ids" : "file_ids"].add(e.id);
    net::Result r = net::postJson(api("/delete"), token, body, resp);
    if (handleAuth(r)) return;
    if (!r.ok()) {
        ui::message("Fehlgeschlagen", r.error, TFT_RED);
        return;
    }
    load(folderId);
}

static void newFolder() {
    String name;
    if (!ui::textInput("Neuer Ordner", name, false, 100) || name.isEmpty()) return;
    ui::busy("Neuer Ordner", "Lege an ...");
    JsonDocument body, resp;
    body["name"] = name;
    if (folderId) body["parent_id"] = folderId;
    else body["parent_id"] = nullptr;
    net::Result r = net::postJson(api("/folders"), token, body, resp);
    if (handleAuth(r)) return;
    if (!r.ok()) {
        ui::message("Fehlgeschlagen", r.error, TFT_RED);
        return;
    }
    load(folderId);
}

// SD-Karte durchsuchen und eine Datei waehlen. Leerer String = abgebrochen.
static String pickSdFile() {
    String dir = "/";
    while (true) {
        std::vector<String> names;
        std::vector<bool> isDir;
        std::vector<String> labels;
        File d = SD.open(dir);
        if (!d || !d.isDirectory()) return "";
        if (dir != "/") {
            names.push_back("..");
            isDir.push_back(true);
            labels.push_back("[..]  zurück");
        }
        std::vector<std::pair<String, uint64_t>> dirs, files;
        for (File f = d.openNextFile(); f; f = d.openNextFile()) {
            String n = f.name();
            int slash = n.lastIndexOf('/');
            if (slash >= 0) n = n.substring(slash + 1);
            if (n.startsWith(".") || n == "System Volume Information") continue;
            if (f.isDirectory()) dirs.push_back({n, 0});
            else files.push_back({n, f.size()});
        }
        d.close();
        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());
        for (auto& x : dirs) {
            names.push_back(x.first);
            isDir.push_back(true);
            labels.push_back("[" + text::sanitize(x.first) + "]");
        }
        for (auto& x : files) {
            names.push_back(x.first);
            isDir.push_back(false);
            labels.push_back(text::sanitize(x.first) + "  (" + text::fmtBytes(x.second) + ")");
        }
        if (names.empty()) {
            ui::message("SD-Karte", "Ordner ist leer.");
            if (dir == "/") return "";
            dir = dir.substring(0, max(1, dir.lastIndexOf('/')));
            continue;
        }
        int i = ui::choose("SD: " + dir, labels);
        if (i < 0) {
            if (dir == "/") return "";
            dir = dir.substring(0, max(1, dir.lastIndexOf('/')));
            continue;
        }
        if (names[i] == "..") {
            dir = dir.substring(0, max(1, dir.lastIndexOf('/')));
        } else if (isDir[i]) {
            dir = (dir == "/" ? "/" : dir + "/") + names[i];
        } else {
            return (dir == "/" ? "/" : dir + "/") + names[i];
        }
    }
}

static void upload() {
    String path = pickSdFile();
    if (path.isEmpty()) return;
    File f = SD.open(path, FILE_READ);
    if (!f) {
        ui::message("Hochladen", "Datei nicht lesbar.", TFT_RED);
        return;
    }
    String name = path.substring(path.lastIndexOf('/') + 1);
    String target = folderPath.isEmpty() ? String("Hauptordner") : folderPath;
    if (!ui::confirm("Hochladen", name + " (" + text::fmtBytes(f.size()) + ")\nnach: " + target)) {
        f.close();
        return;
    }
    JsonDocument resp;
    net::Result r = net::uploadFile(api("/upload"), token, "file", f, name, folderId ? "folder_id" : "",
                                    String(folderId), resp, [&](uint64_t done, uint64_t total) {
                                        ui::progress("Hochladen", name, done, total);
                                    });
    f.close();
    if (handleAuth(r)) return;
    if (!r.ok()) {
        ui::message("Hochladen fehlgeschlagen", r.error, TFT_RED);
        return;
    }
    load(folderId);
    ui::message("Hochgeladen", name, TFT_GREEN);
}

static void info() {
    ui::busy("Konto", "Lade ...");
    JsonDocument doc;
    net::Result r = net::getJson(api("/me"), token, doc);
    if (handleAuth(r)) return;
    if (!r.ok()) {
        ui::message("Konto", r.error, TFT_RED);
        return;
    }
    JsonObject st = doc["storage"];
    String s = "Angemeldet als " + text::sanitize(doc["username"] | "") + "\n";
    s += String("2FA: ") + ((doc["totp_enabled"] | false) ? "aktiv" : "aus") + "\n\n";
    if (String(st["mode"] | "") == "quota") {
        s += "Belegt: " + text::fmtDecimal(st["used_gb"] | 0.0, 2) + " von " + text::fmtDecimal(st["total_gb"] | 0.0, 2) + " GB\n";
    } else {
        s += "Eigene Dateien: " + text::fmtDecimal(st["own_gb"] | 0.0, 2) + " GB\n";
        s += "Platte: " + text::fmtDecimal(st["used_gb"] | 0.0, 1) + " von " + text::fmtDecimal(st["total_gb"] | 0.0, 1) + " GB\n";
    }
    s += "Frei: " + text::fmtDecimal(st["free_gb"] | 0.0, 1) + " GB (" + text::fmtDecimal(st["percent_used"] | 0.0, 1) + " % belegt)";
    ui::message("Konto", s);
}

// Wird aus den Einstellungen aufgerufen
void filesLogout() {
    if (!loggedIn()) return;
    ui::busy("Abmelden", "Melde ab ...");
    JsonDocument body, resp;
    net::postJson(api("/logout"), token, body, resp);  // Token serverseitig loeschen; Fehler egal
    forgetLogin();
}
String filesUser() { return user; }
String filesToken() { return token; }
void filesForget() { forgetLogin(); }

// ---------- App ----------

static void enter() {
    if (configured() && loggedIn() && !loaded) load(folderId);
}

static void draw() {
    ui::clear();
    ui::drawHeader("Dateien");
    auto& c = ui::canvas;
    const int w = c.width();

    auto centered = [&](const String& t) {
        c.setTextColor(TFT_LIGHTGREY);
        int y = ui::contentTop() + 6;
        for (auto& l : ui::wrap(t, w - 8)) {
            c.drawString(l, 4, y);
            y += ui::LINE_H;
        }
    };
    if (!configured()) {
        centered("Nicht eingerichtet. In /raveneye/config.txt eintragen:\ncloud_url=https://...");
        return;
    }
    if (!loggedIn()) {
        centered("Nicht angemeldet.\n\nEnter: Anmelden");
        return;
    }

    // Pfad unter der Kopfzeile
    int y0 = ui::contentTop();
    c.setTextColor(TFT_CYAN);
    c.drawString(ui::fitLine("/ " + folderPath, w - 8), 4, y0);
    y0 += ui::LINE_H;

    ui::drawHint(lastError.isEmpty() ? "Enter  u:Hoch  n:Ordner  x:Löschen  i:Info" : "Fehler: " + lastError);
    if (entries.empty()) {
        c.setTextColor(ui::C_HINT);
        const char* t = loaded ? "Ordner ist leer" : "Nicht geladen - r: neu laden";
        c.drawString(t, (w - c.textWidth(t)) / 2, y0 + 2 * ui::LINE_H);
        return;
    }
    const int rowH = ui::LINE_H + 1;
    ui::drawList(entries.size(), sel, top, y0, ui::contentBottom(true), rowH, [&](int i, int y, bool) {
        const auto& e = entries[i];
        String size = text::fmtSizeMb(e.sizeMb);
        int sw = c.textWidth(size);
        if (e.folder) {
            c.setTextColor(TFT_YELLOW);
            c.setFont(&ui::FONT_BOLD);
            c.drawString(ui::fitLine(e.name + "/", w - 20 - sw), 6, y);
            c.setFont(&ui::FONT);
        } else {
            c.setTextColor(TFT_WHITE);
            c.drawString(ui::fitLine(e.name, w - 20 - sw), 6, y);
        }
        c.setTextColor(ui::C_HINT);
        c.drawString(size, w - 8 - sw, y);
    });
}

static bool key(const Keys& k) {
    if (!configured()) return !k.back;
    if (!loggedIn()) {
        if (k.enter) {
            login();
            if (loggedIn()) load(0);
        }
        return !k.back;
    }
    if (k.back) {
        if (folderId == 0) return false;
        load(parentId);
        return true;
    }
    if (k.has('r')) load(folderId);
    else if (k.has('u')) upload();
    else if (k.has('n')) newFolder();
    else if (k.has('i')) info();
    if (entries.empty()) return true;

    if (k.up && sel > 0) sel--;
    else if (k.down && sel + 1 < (int)entries.size()) sel++;
    else if (k.left) sel = max(0, sel - 5);
    else if (k.right) sel = min((int)entries.size() - 1, sel + 5);
    else if (k.has('x')) trash(entries[sel]);
    else if (k.enter) {
        Entry e = entries[sel];  // Kopie: load() ersetzt die Liste
        if (e.folder) {
            load(e.id);
            return true;
        }
        std::vector<String> opts = {"Auf SD-Karte laden"};
        bool canView = isText(e) && e.sizeMb <= 1.0;
        if (canView) opts.push_back("Anzeigen");
        opts.push_back("In den Papierkorb");
        int choice = ui::choose(ui::fitLine(e.name, ui::width() - 90) + "  " + text::fmtSizeMb(e.sizeMb), opts);
        if (choice == 0) download(e);
        else if (canView && choice == 1) preview(e);
        else if (choice == (canView ? 2 : 1)) trash(e);
    }
    return true;
}

static String badge() {
    if (!configured()) return "nicht eingerichtet";
    return loggedIn() ? user : String("abgemeldet");
}

App filesApp = {"Dateien", enter, key, draw, nullptr, badge};
