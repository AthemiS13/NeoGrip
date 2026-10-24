#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include "esp_random.h"

// Connect the phone to NeoGrip-L-xxxxxx / NeoGrip-R-xxxxxx with this password.
// The access point exists only during setup/recovery, and closes after success.
const char* setupAPPassword = "neogripsetup";

const char setupPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>NeoGrip Wi-Fi setup</title><style>
*{box-sizing:border-box}body{margin:0;background:#101824;color:#edf4ff;font:17px system-ui,sans-serif;padding:28px 18px}
main{max-width:440px;margin:5vh auto}small{color:#88c9ff;letter-spacing:.12em}h1{font-size:32px;line-height:1.1}
p{line-height:1.5;color:#b7c8dd}label{display:block;margin:20px 0 8px}input,button{font:inherit;width:100%;border-radius:10px;padding:14px}
input{background:#1b293c;color:white;border:1px solid #52677f}button{margin-top:24px;background:#8bdbb0;color:#10261c;border:0;font-weight:700;cursor:pointer}
input[type=checkbox]{width:auto;margin-right:8px}.hint{font-size:14px}#status{padding:14px;border-radius:10px;background:#1b293c;min-height:52px}
</style></head><body><main><small>NEOGRIP / {{SIDE}} CONTROLLER</small><h1>Connect to your Wi-Fi</h1>
<p>Choose the same 2.4 GHz network as your VR computer. Your current saved network is kept until the new connection succeeds.</p>
<form id="setup" action="/save" method="post"><input type="hidden" name="token" value="{{TOKEN}}">
<label for="ssid">Wi-Fi name (SSID)</label><input id="ssid" name="ssid" value="{{SSID}}" required maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false">
<label for="password">Wi-Fi password</label><input id="password" name="password" type="password" maxlength="64" autocomplete="new-password">
<label class="hint"><input type="checkbox" id="show">Show password</label>
<p class="hint">Leave the password empty only for an open network.</p><button type="submit">Connect and save</button></form>
<p id="status" role="status" aria-live="polite">Ready for setup.</p>
<p class="hint">If this page closes, open http://192.168.4.1 while connected to the NeoGrip access point.</p>
<script>
const statusBox=document.getElementById('status');let submitted=false;
document.getElementById('show').onchange=e=>document.getElementById('password').type=e.target.checked?'text':'password';
document.getElementById('setup').onsubmit=async e=>{e.preventDefault();try{
const response=await fetch('/save',{method:'POST',body:new URLSearchParams(new FormData(e.target))});
statusBox.textContent=await response.text();submitted=response.ok;
}catch(error){statusBox.textContent='Connection interrupted. Stay on the NeoGrip Wi-Fi and reload this page.';}};
setInterval(async()=>{try{const response=await fetch('/status',{cache:'no-store'});const data=await response.json();
if(data.state==='connected')statusBox.textContent='Connected and saved. Rejoin your home Wi-Fi. NeoGrip setup will close shortly.';
else if(data.state==='unsaved')statusBox.textContent='Connected, but saving failed. Please submit again to retry.';
else if(submitted&&data.state==='failed')statusBox.textContent='Could not connect yet. Check the Wi-Fi name and password. Your previous saved network is unchanged.';
else if(submitted)statusBox.textContent='Connecting to your Wi-Fi…';
}catch(error){}},1500);
</script></main></body></html>)HTML";

class WiFiSetup {
public:
    void begin(const char* side) {
        side_ = side;
        char name[32];
        snprintf(name, sizeof(name), "NeoGrip-%s-%06lx", side,
                 (unsigned long)((ESP.getEfuseMac() >> 24) & 0xffffff));
        apName_ = name;
        char token[17];
        snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
        token_ = token;
        preferencesReady_ = preferences_.begin("neogrip", false);
        Credentials saved = {};
        if (preferencesReady_ && preferences_.getBytesLength("wifi") == sizeof(saved) &&
            preferences_.getBytes("wifi", &saved, sizeof(saved)) == sizeof(saved) &&
            memchr(saved.ssid, '\0', sizeof(saved.ssid)) && memchr(saved.password, '\0', sizeof(saved.password))) {
            ssid_ = saved.ssid;
            password_ = saved.password;
        }
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false);
        offlineSince_ = lastAttemptAt_ = millis();
        lastNetworkPollAt_ = millis() - 100;
        lastPortalAttemptAt_ = millis() - 5000;
        if (ssid_.length()) WiFi.begin(ssid_.c_str(), password_.c_str());
        else startPortal();
    }

    bool portalActive() const { return portalActive_; }

    void suspend() {
        if (portalActive_) stopPortal();
        WiFi.disconnect(true, false);
    }

    void resume() {
        wasConnected_ = false;
        offlineSince_ = lastAttemptAt_ = millis();
        lastNetworkPollAt_ = lastPortalAttemptAt_ = millis() - 5000;
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false);
        if (ssid_.length()) WiFi.begin(ssid_.c_str(), password_.c_str());
        else startPortal();
    }

    void retryConnection() {
        // Share the retry clock with normal Wi-Fi recovery to avoid two loops
        // repeatedly interrupting an association attempt.
        lastAttemptAt_ = millis();
        WiFi.reconnect();
    }

    void service(uint32_t now) {
        if (portalActive_) {
            dns_.processNextRequest();
            server_.handleClient();
        }
        now = millis();
        // Respond to the phone before changing the radio's station connection.
        if (applyPending_ && uint32_t(now - submittedAt_) >= 500) {
            applyPending_ = false;
            ssid_ = pendingSSID_;
            password_ = pendingPassword_;
            saveOnConnect_ = true;
            saveFailed_ = false;
            wasConnected_ = false;
            WiFi.disconnect(false, false);
            WiFi.begin(ssid_.c_str(), password_.c_str());
            offlineSince_ = lastAttemptAt_ = now;
            lastNetworkPollAt_ = now - 100;
        }
        if (uint32_t(now - lastNetworkPollAt_) < 100) return;
        lastNetworkPollAt_ = now;
        const bool connected = WiFi.status() == WL_CONNECTED && (!saveOnConnect_ || WiFi.SSID() == ssid_);
        if (connected) {
            if (!wasConnected_) {
                connectedAt_ = now;
                WiFi.setSleep(false);
            }
            wasConnected_ = true;
            if (saveOnConnect_) {
                Credentials saved = {};
                ssid_.toCharArray(saved.ssid, sizeof(saved.ssid));
                password_.toCharArray(saved.password, sizeof(saved.password));
                saveFailed_ = !preferencesReady_ || preferences_.putBytes("wifi", &saved, sizeof(saved)) != sizeof(saved);
                saveOnConnect_ = false;
                Serial.println(saveFailed_ ? "[SETUP] Wi-Fi connected, but saving failed." : "[SETUP] Wi-Fi connected and saved.");
            }
            if (portalActive_ && !applyPending_ && !saveFailed_ && uint32_t(now - connectedAt_) >= 15000) stopPortal();
        } else {
            if (wasConnected_) offlineSince_ = now;
            wasConnected_ = false;
            if (!ssid_.length() || uint32_t(now - offlineSince_) >= 20000) {
                if (!portalActive_ && uint32_t(now - lastPortalAttemptAt_) >= 5000) {
                    lastPortalAttemptAt_ = now;
                    startPortal();
                }
            }
            if (ssid_.length() && uint32_t(now - lastAttemptAt_) >= 10000 && !applyPending_) {
                lastAttemptAt_ = now;
                WiFi.reconnect();
            }
        }
    }

private:
    struct Credentials { char ssid[33]; char password[65]; };
    Preferences preferences_;
    DNSServer dns_;
    WebServer server_{80};
    String side_, apName_, token_, ssid_, password_, pendingSSID_, pendingPassword_;
    bool preferencesReady_ = false;
    bool portalActive_ = false;
    bool handlersReady_ = false;
    bool applyPending_ = false;
    bool saveOnConnect_ = false;
    bool saveFailed_ = false;
    bool wasConnected_ = false;
    uint32_t offlineSince_ = 0, lastAttemptAt_ = 0, lastPortalAttemptAt_ = 0;
    uint32_t connectedAt_ = 0, submittedAt_ = 0;
    uint32_t lastNetworkPollAt_ = 0;

    static String escapeHTML(String value) {
        value.replace("&", "&amp;"); value.replace("<", "&lt;");
        value.replace(">", "&gt;"); value.replace("\"", "&quot;");
        value.replace("'", "&#39;");
        return value;
    }

    bool fromSetupNetwork() {
        if (server_.client().localIP() == WiFi.softAPIP()) return true;
        server_.send(403, "text/plain", "Connect to the NeoGrip setup Wi-Fi first.");
        return false;
    }

    void showPage() {
        if (!fromSetupNetwork()) return;
        String html = FPSTR(setupPage);
        html.replace("{{SIDE}}", escapeHTML(side_));
        html.replace("{{SSID}}", escapeHTML(ssid_));
        html.replace("{{TOKEN}}", token_);
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "text/html; charset=utf-8", html);
    }

    void save() {
        if (!fromSetupNetwork()) return;
        if (server_.arg("token") != token_) {
            server_.send(403, "text/plain", "Please reload the setup page and try again.");
            return;
        }
        const String name = server_.arg("ssid"), password = server_.arg("password");
        bool validPassword = password.length() == 0 || (password.length() >= 8 && password.length() <= 63);
        if (password.length() == 64) {
            validPassword = true;
            for (unsigned int i = 0; i < 64; ++i) {
                char c = password[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) validPassword = false;
            }
        }
        // SSID length is in UTF-8 bytes, not characters. Preserve spaces exactly.
        if (!name.length() || name.length() > 32 || !validPassword ||
            strlen(name.c_str()) != name.length() || strlen(password.c_str()) != password.length()) {
            server_.send(400, "text/plain", "Use a Wi-Fi name of 1-32 bytes and a password of 8-63 characters (or empty for an open network).");
            return;
        }
        pendingSSID_ = name;
        pendingPassword_ = password;
        submittedAt_ = millis();
        applyPending_ = true;
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(202, "text/plain", "Connecting. Stay on the NeoGrip Wi-Fi while we check the new network.");
    }

    void status() {
        if (!fromSetupNetwork()) return;
        const char* state = applyPending_ ? "connecting" : wasConnected_ ? (saveFailed_ ? "unsaved" : "connected") :
            uint32_t(millis() - offlineSince_) >= 20000 ? "failed" : "connecting";
        String json = "{\"state\":\"";
        json += state; json += "\"}";
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "application/json", json);
    }

    void startPortal() {
        WiFi.mode(WIFI_AP_STA);
        if (!WiFi.softAP(apName_.c_str(), setupAPPassword)) {
            Serial.println("[SETUP] Could not start access point; will retry.");
            return;
        }
        if (!handlersReady_) {
            server_.on("/", HTTP_GET, [this]() { showPage(); });
            server_.on("/save", HTTP_POST, [this]() { save(); });
            server_.on("/status", HTTP_GET, [this]() { status(); });
            server_.onNotFound([this]() { showPage(); });
            handlersReady_ = true;
        }
        dns_.start(53, "*", WiFi.softAPIP());
        server_.begin();
        portalActive_ = true;
        Serial.printf("[SETUP] Connect to %s; open http://192.168.4.1\n", apName_.c_str());
    }

    void stopPortal() {
        dns_.stop();
        server_.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        portalActive_ = false;
        Serial.println("[SETUP] Access point closed.");
    }
};
