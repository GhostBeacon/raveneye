#include "auth.h"

#include <ArduinoJson.h>

#include "core.h"
#include "net.h"
#include "ui.h"

namespace auth {

static String tok, name;

static String api(const String& path) { return core::cfg.serverUrl + "/api/v1" + path; }

void begin() {
    tok = core::prefs.getString("srv_token", "");
    name = core::prefs.getString("srv_user", "");
}

bool loggedIn() { return !tok.isEmpty(); }
String token() { return tok; }
String user() { return name; }

void forget() {
    tok = "";
    core::prefs.remove("srv_token");
}

void login() {
    if (core::cfg.serverUrl.isEmpty()) {
        ui::message("Anmelden", "server_url fehlt in /raveneye/config.txt.", ui::C_WARN);
        return;
    }
    String u = name, pass, code;
    if (!ui::textInput("Benutzername", u, false, 64) || u.isEmpty()) return;
    if (!ui::textInput("Passwort", pass, true, 128) || pass.isEmpty()) return;

    while (true) {
        ui::busy("Anmelden", "Verbinde ...");
        JsonDocument body, resp;
        body["username"] = u;
        body["password"] = pass;
        body["device_name"] = "RavenEye";
        body["scope"] = "monitor";  // nur Systemwerte, keine Dateien
        if (!code.isEmpty()) body["totp_code"] = code;
        net::Result r = net::postJson(api("/login"), "", body, resp);

        if (r.ok()) {
            String t = resp["token"] | "";
            // Aelterer Server ignoriert "scope" und gibt ein Voll-Token aus -> sofort verwerfen
            if (String(resp["scope"] | "") != "monitor") {
                JsonDocument none, ignored;
                net::postJson(api("/logout"), t, none, ignored);
                ui::message("Anmelden abgebrochen",
                            "Der Server vergibt noch keine Nur-Lese-Tokens. Bitte zuerst das Server-Update einspielen.",
                            ui::C_ERR);
                break;
            }
            tok = t;
            name = resp["user"]["username"] | u.c_str();
            break;
        }
        if (r.code == "totp_required" || r.code == "invalid_totp") {
            if (r.code == "invalid_totp") ui::message("Anmelden", r.error, ui::C_WARN);
            code = "";
            if (!ui::textInput("2FA-Code", code, false, 8) || code.isEmpty()) break;
            continue;
        }
        String msg = r.error;
        if (!resp["remaining_attempts"].isNull()) msg += "\nNoch " + String((int)resp["remaining_attempts"]) + " Versuche.";
        ui::message("Anmelden fehlgeschlagen", msg, ui::C_ERR);
        break;
    }
    for (unsigned i = 0; i < pass.length(); i++) pass.setCharAt(i, 0);

    if (!tok.isEmpty()) {
        core::prefs.putString("srv_token", tok);
        core::prefs.putString("srv_user", name);
    }
}

void logout() {
    if (!loggedIn()) return;
    ui::busy("Abmelden", "Melde ab ...");
    JsonDocument body, resp;
    net::postJson(api("/logout"), tok, body, resp);  // Token serverseitig loeschen; Fehler egal
    forget();
}

}  // namespace auth
