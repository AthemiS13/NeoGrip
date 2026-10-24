#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "esp_ota_ops.h"
#include <errno.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "WiFiSetup.h"
#include "MotorFeedback.h"

// Arduino-ESP32 3.x. Wi-Fi is configured from the phone setup portal.
const char* controllerType = "L"; // Use "L" on the left controller.
// Optional fixed OTA password (8-64 bytes). Empty generates a saved, per-device
// password, printed in Serial Monitor at boot. It survives firmware updates.
const char* otaPassword = "";

const int buttonA = 21;
const int buttonB = 18;
const int buttonSYS = 4;
const int buttonJOYCLK = 14;
const int buttonTRIG = 19;
const int buttonSQZ = 27;
const int joyXPin = 34;
const int joyYPin = 35;
const int motorPin = 16;

// Set for the actual motor driver wiring, independently of controllerType.
// true: active-low (typically PNP); false: active-high (typically NPN).
const bool motorActiveLow = true;
const int maxDuty = 255;
const uint32_t sendInterval = 40;    // 25 Hz, matching the original working transport.
const uint32_t restartHoldMs = 7000;
const uint32_t socketRetryMs = 1000;
const uint32_t startTimeoutMs = 60000;
const unsigned int localPort = 9999; // Destination on the PC, not a second local bind.
const unsigned int hapticPort = 8888;

WiFiUDP udp;
WiFiUDP inputUdp; // Keep input transmission independent of the haptic listener.
WiFiSetup wifiSetup;
MotorFeedback feedback(motorPin, motorActiveLow);
IPAddress boundIP;
IPAddress proxyIP;
bool socketReady = false;
bool receivedStart = false;
bool sleepPending = false;
bool watchdogReady = false;
bool restartArmed = true;
bool restartHeld = false;
bool wifiWasConnected = false;
uint32_t restartStartedAt = 0;
uint32_t previousSendAt = 0;
uint32_t lastSocketAttemptAt = 0;
uint32_t waitingSince = 0;
uint32_t lastProxyPacketAt = 0;
unsigned int sendFailures = 0;
uint32_t sendBackoffAt = 0;
uint32_t sendBackoffMs = 0;
uint32_t lastSendErrorAt = 0;
bool sendErrorLogged = false;
uint32_t lastNetworkCheckAt = 0;

bool restartPending = false;
bool wifiLossPending = false;
bool wifiLossNotified = false;
bool proxyLossNotified = false;
bool portalWasActive = false;
bool txFailureActive = false;
bool txLossNotified = false;
bool motorSupplyRetained = false;
uint32_t wifiLostAt = 0;
uint32_t txFailureAt = 0;
bool inputAckExpected = false;
bool inputLossNotified = false;
uint32_t lastInputAckAt = 0;
uint32_t lastRxRepairAt = 0;
uint32_t lastTxRepairAt = 0;
bool otaConfigured = false;
bool otaReady = false;
bool otaInProgress = false;
bool otaFailed = false;
bool otaCompleted = false;
bool otaWatchdogPaused = false;
uint32_t lastOTAHandleAt = 0;
IPAddress otaIP;
char otaHostname[32] = {};
char otaCredential[65] = {};

void stopOTA() {
    if (otaReady) ArduinoOTA.end();
    otaReady = false;
}
portMUX_TYPE wifiEventLock = portMUX_INITIALIZER_UNLOCKED;
volatile bool wifiDisconnected = false;
volatile uint8_t wifiDisconnectReason = 0;

void onWiFiDisconnected(WiFiEvent_t, WiFiEventInfo_t info) {
    // The Wi-Fi task only records the event; socket and motor work stays in loop().
    portENTER_CRITICAL(&wifiEventLock);
    wifiDisconnectReason = info.wifi_sta_disconnected.reason;
    wifiDisconnected = true;
    portEXIT_CRITICAL(&wifiEventLock);
}

void startHaptic(int strength, uint32_t duration, uint32_t now) {
    feedback.gamePulse(strength, duration, now);
}

void serviceRestartGesture(uint32_t now) {
    const bool menu = digitalRead(buttonSYS) == LOW;
    const bool trigger = digitalRead(buttonTRIG) == LOW;
    if (!restartArmed) {
        // Prevent another reboot if the user keeps holding through startup.
        if (!menu && !trigger) restartArmed = true;
        return;
    }
    if (!menu || !trigger) {
        restartHeld = false;
        return;
    }
    if (!restartHeld) {
        restartHeld = true;
        restartStartedAt = now;
    } else if (uint32_t(now - restartStartedAt) >= restartHoldMs) {
        if (!restartPending) {
            restartPending = true;
            stopOTA();
            udp.stop(); inputUdp.stop();
            feedback.notify(FeedbackEvent::Restart, now);
            Serial.println("[SYSTEM] Menu + trigger held for 7 seconds; restarting this controller.");
        }
    }
}

void configureWatchdog() {
    esp_task_wdt_config_t config = {};
    config.timeout_ms = 10000;
    config.idle_core_mask = 0;
    config.trigger_panic = true;
    esp_err_t result = esp_task_wdt_init(&config);
    if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_reconfigure(&config);
    if (result == ESP_OK) {
        result = esp_task_wdt_status(nullptr);
        if (result != ESP_OK) result = esp_task_wdt_add(nullptr);
    }
    watchdogReady = result == ESP_OK;
    if (!watchdogReady) Serial.printf("[ERROR] Watchdog setup failed: %d\n", result);
}

void closeSocket() {
    udp.stop();
    inputUdp.stop();
    socketReady = false;
    receivedStart = false;
    sendFailures = 0;
    sendBackoffMs = 0;
    txFailureActive = txLossNotified = false;
    inputAckExpected = inputLossNotified = false;
    lastInputAckAt = lastRxRepairAt = lastTxRepairAt = millis();
    feedback.stopGame();
}

void configureOTA() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!running || !next || running->address == next->address) {
        Serial.println("[OTA] Disabled: select a partition scheme with two OTA app slots and flash once by USB.");
        return;
    }
    const size_t configuredLength = strlen(otaPassword);
    if (configuredLength && (configuredLength < 8 || configuredLength > 64)) {
        Serial.println("[OTA] Disabled: otaPassword must contain 8-64 bytes, or be empty for an automatic password.");
        return;
    }
    Preferences credentials;
    if (!credentials.begin("neogrip-ota", false)) {
        Serial.println("[OTA] Disabled: could not open password storage.");
        return;
    }
    const bool loaded = credentials.getBytesLength("password") == sizeof(otaCredential) &&
        credentials.getBytes("password", otaCredential, sizeof(otaCredential)) == sizeof(otaCredential) &&
        memchr(otaCredential, '\0', sizeof(otaCredential));
    const bool valid = loaded && strlen(otaCredential) >= 8 && strlen(otaCredential) <= 64;
    bool save = false;
    if (configuredLength) {
        save = !valid || strcmp(otaCredential, otaPassword) != 0;
        memset(otaCredential, 0, sizeof(otaCredential));
        memcpy(otaCredential, otaPassword, configuredLength);
    } else if (!valid) {
        memset(otaCredential, 0, sizeof(otaCredential));
        snprintf(otaCredential, sizeof(otaCredential), "%08lx%08lx%08lx%08lx",
                 (unsigned long)esp_random(), (unsigned long)esp_random(),
                 (unsigned long)esp_random(), (unsigned long)esp_random());
        save = true;
    }
    if (save && credentials.putBytes("password", otaCredential, sizeof(otaCredential)) != sizeof(otaCredential)) {
        credentials.end();
        Serial.println("[OTA] Disabled: could not save the upload password.");
        return;
    }
    credentials.end();
    snprintf(otaHostname, sizeof(otaHostname), "NeoGrip-%s-%06lx", controllerType,
             (unsigned long)((ESP.getEfuseMac() >> 24) & 0xffffff));
    ArduinoOTA.setHostname(otaHostname);
    ArduinoOTA.setPassword(otaCredential);
    ArduinoOTA.setTimeout(5000);
    ArduinoOTA.setRebootOnSuccess(false); // Reboot here only after handle() has returned.
    ArduinoOTA.onStart([]() {
        otaInProgress = true;
        otaFailed = otaCompleted = false;
        // Flash writes must never happen with PWM driving the motor. Hold the
        // physical OFF level and stop the safety timer before the first write.
        closeSocket();
        if (!feedback.prepareStandby() || ArduinoOTA.getCommand() != U_FLASH) {
            otaFailed = true;
            Update.abort();
            Serial.println("[OTA] Update refused: motor OFF could not be held, or this is not a firmware upload.");
            return;
        }
        if (watchdogReady) {
            if (esp_task_wdt_delete(nullptr) != ESP_OK) {
                otaFailed = true;
                Update.abort();
                Serial.println("[OTA] Update refused: could not suspend the loop watchdog.");
                return;
            }
            otaWatchdogPaused = true;
            watchdogReady = false;
        }
        Serial.println("[OTA] Arduino IDE upload started; motor held OFF.");
    });
    ArduinoOTA.onProgress([](unsigned int, unsigned int) {
        // Normally unsubscribed during the blocking transfer. Keep feeding if
        // an aborted start left the original watchdog subscription in place.
        if (watchdogReady) esp_task_wdt_reset();
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Upload failed: error=%u. Current firmware retained.\n", (unsigned int)error);
        if (otaInProgress) {
            otaFailed = true;
            Update.abort();
        }
        // Do not resume hardware here: ArduinoOTA may still be unwinding a
        // failed write and can invoke this callback more than once.
    });
    ArduinoOTA.onEnd([]() {
        otaCompleted = true;
        Serial.println("[OTA] Firmware verified; restarting.");
    });
    otaConfigured = true;
    Serial.printf("[OTA] Arduino IDE name: %s.local; upload password: %s\n", otaHostname, otaCredential);
}

void serviceOTA() {
    if (!otaConfigured) return;
    const bool stationConnected = WiFi.status() == WL_CONNECTED;
    if (!stationConnected && !wifiSetup.portalActive()) { stopOTA(); return; }
    const IPAddress currentIP = stationConnected ? WiFi.localIP() : WiFi.softAPIP();
    if (otaReady && currentIP != otaIP) stopOTA();
    if (!otaReady) {
        otaIP = currentIP;
        ArduinoOTA.begin();
        otaReady = true;
        lastOTAHandleAt = millis() - 10;
        Serial.printf("[OTA] Network uploads available as %s.local (port 3232).\n", otaHostname);
    }
    if (uint32_t(millis() - lastOTAHandleAt) < 10) return;
    lastOTAHandleAt = millis();
    ArduinoOTA.handle();
    if (otaCompleted) {
        // The motor stays held OFF across the reboot into the verified image.
        ESP.restart();
        return;
    }
    if (otaInProgress && otaFailed) {
        feedback.resume();
        otaInProgress = otaFailed = false;
        waitingSince = millis();
        if (otaWatchdogPaused) {
            otaWatchdogPaused = false;
            configureWatchdog();
        }
        Serial.println("[OTA] Controller recovered; ready to reconnect or retry the upload.");
    }
}

void requestSleep() {
    if (sleepPending) return;
    stopOTA();
    closeSocket();
    sleepPending = true;
    feedback.notify(FeedbackEvent::Sleep, millis());
    Serial.println("[SYSTEM] Sleep cue; waiting for system button release.");
}

void serviceSleep() {
    if (feedback.notificationActive() || digitalRead(buttonSYS) == LOW) return;
    if (!feedback.prepareStandby()) {
        Serial.println("[ERROR] Motor OFF could not be confirmed; staying awake.");
        delay(100);
        return;
    }
    wifiSetup.suspend();
    rtc_gpio_pullup_en((gpio_num_t)buttonSYS);
    rtc_gpio_pulldown_dis((gpio_num_t)buttonSYS);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonSYS, LOW);

    // GPIO16/17 on ESP32 use VDD_SDIO. Deep-sleep forcibly powers that
    // domain off even if requested ON, which can turn an active-low motor on.
    // Use light-sleep with the supply retained instead of relying on pad hold.
    const bool needsPinSupply = motorPin == 16 || motorPin == 17;
    if (needsPinSupply) {
        const esp_err_t powerResult = motorSupplyRetained ? ESP_OK :
            esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON);
        if (powerResult != ESP_OK) {
            Serial.println("[ERROR] Motor pin supply retention failed; staying awake.");
            delay(100);
            return;
        }
        motorSupplyRetained = true;
        if (watchdogReady) esp_task_wdt_reset();
        const esp_err_t sleepResult = esp_light_sleep_start();
        if (watchdogReady) esp_task_wdt_reset();
        if (sleepResult != ESP_OK || esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
        rtc_gpio_deinit((gpio_num_t)buttonSYS);
        pinMode(buttonSYS, INPUT_PULLUP);
        feedback.resume();
        sleepPending = false;
        wifiWasConnected = false;
        wifiLossPending = wifiLossNotified = proxyLossNotified = false;
        portalWasActive = false;
        waitingSince = millis();
        wifiSetup.resume();
        feedback.notify(FeedbackEvent::Wake, millis());
        Serial.println("[SYSTEM] Woken by system button.");
    } else {
        gpio_deep_sleep_hold_en();
        if (watchdogReady) esp_task_wdt_reset();
        esp_deep_sleep_start();
    }
}

void serviceNetwork(uint32_t now) {
    portENTER_CRITICAL(&wifiEventLock);
    const bool disconnected = wifiDisconnected;
    const uint8_t reason = wifiDisconnectReason;
    wifiDisconnected = false;
    portEXIT_CRITICAL(&wifiEventLock);
    if (disconnected && (wifiWasConnected || socketReady)) {
        // A disconnect/reconnect may complete between 100 ms status polls.
        // Rebind even when the station has already recovered the same IP.
        Serial.printf("[WIFI] Station disconnected: reason=%u; reopening transport.\n", reason);
        wifiLossPending = true;
        wifiLossNotified = false;
        wifiLostAt = waitingSince = now;
        stopOTA();
        closeSocket();
        wifiWasConnected = false;
    }
    if (socketReady && uint32_t(now - lastNetworkCheckAt) < 100) return;
    lastNetworkCheckAt = now;
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiWasConnected || socketReady) {
            Serial.println("[WIFI] Connection lost; retrying.");
            wifiLossPending = true;
            wifiLossNotified = false;
            wifiLostAt = now;
            stopOTA();
            closeSocket();
            waitingSince = now;
        }
        wifiWasConnected = false;
        if (wifiLossPending && !wifiLossNotified && uint32_t(now - wifiLostAt) >= 3000) {
            feedback.notify(FeedbackEvent::WiFiLost, now);
            wifiLossNotified = true;
        }
        return;
    }

    const IPAddress currentIP = WiFi.localIP();
    if (!wifiWasConnected || currentIP != boundIP) {
        stopOTA();
        closeSocket();
        boundIP = currentIP;
        wifiWasConnected = true;
        wifiLossPending = wifiLossNotified = false;
        feedback.notify(FeedbackEvent::WiFiReady, now);
        waitingSince = now;
        lastSocketAttemptAt = now - socketRetryMs;
        WiFi.setSleep(false);
        Serial.print("[WIFI] Connected: ");
        Serial.println(currentIP);
    }
    if (!socketReady && uint32_t(now - lastSocketAttemptAt) >= socketRetryMs) {
        lastSocketAttemptAt = now;
        socketReady = udp.begin(hapticPort) == 1;
        if (socketReady) {
            lastRxRepairAt = now;
            Serial.println(receivedStart ? "[UDP] Listener reopened; input streaming continues." :
                                         "[UDP] Listening on 8888; waiting for START.");
        }
        else Serial.println("[UDP] Bind failed; will retry.");
    }
}

void receivePackets() {
    // Bound each pass so a packet flood cannot starve buttons, timers or the WDT.
    for (int count = 0; count < 16 && socketReady; ++count) {
        const int packetSize = udp.parsePacket();
        if (packetSize <= 0) break;
        const IPAddress sender = udp.remoteIP();
        char packet[16];
        const int length = udp.read(packet, sizeof(packet) - 1);
        // Critical: unread bytes make NetworkUDP::parsePacket() return 0 forever.
        // Always discard the remainder, including oversized/malformed datagrams.
        udp.clear();
        if (length <= 0 || length != packetSize) continue;
        packet[length] = '\0';
        const uint32_t now = millis();

        if (length == 5 && memcmp(packet, "START", 5) == 0) {
            if (receivedStart && sender != proxyIP && uint32_t(now - lastProxyPacketAt) < 5000) continue;
            if (!receivedStart || sender != proxyIP) {
                inputAckExpected = inputLossNotified = false;
                lastInputAckAt = lastTxRepairAt = now;
            }
            if (!receivedStart || proxyLossNotified) {
                Serial.println("[SYSTEM] START received; streaming.");
                feedback.notify(FeedbackEvent::Connected, now);
            }
            proxyLossNotified = false;
            // Repeated START is a heartbeat, never a repeated startup vibration.
            proxyIP = sender;
            receivedStart = true;
            lastProxyPacketAt = now;
            continue;
        }
        if (!receivedStart || sender != proxyIP) continue;
        if (length == 4 && memcmp(packet, "ACK?", 4) == 0) {
            // Capability announcement, not a receipt. Repeated announcements
            // must not refresh the last successful input acknowledgement.
            if (!inputAckExpected) {
                inputAckExpected = true;
                lastInputAckAt = now;
            }
            lastProxyPacketAt = now;
            continue;
        }
        if (length == 4 && memcmp(packet, "ACK", 3) == 0 && packet[3] == controllerType[0]) {
            // START only proves PC -> controller. ACK proves this hand's input
            // reached the proxy; a successful UDP send alone does not prove it.
            inputAckExpected = true;
            lastInputAckAt = lastProxyPacketAt = now;
            if (inputLossNotified) {
                Serial.println("[UDP] Proxy confirmed input reception again.");
                inputLossNotified = false;
                feedback.notify(FeedbackEvent::Connected, now);
            }
            continue;
        }
        if (length == 4 && memcmp(packet, "STOP", 4) == 0) {
            requestSleep();
            return;
        }
        if (length != 7 || packet[0] != controllerType[0]) continue;
        bool digits = true;
        for (int i = 1; i < 7; ++i) {
            if (packet[i] < '0' || packet[i] > '9') digits = false;
        }
        if (!digits) continue;
        const int strength = (packet[1] - '0') * 100 + (packet[2] - '0') * 10 + packet[3] - '0';
        const uint32_t duration = (packet[4] - '0') * 100 + (packet[5] - '0') * 10 + packet[6] - '0';
        if (strength > maxDuty) continue;
        lastProxyPacketAt = now;
        startHaptic(strength, duration, now);
    }
}

void serviceTransport(uint32_t now) {
    if (!wifiWasConnected) return;
    const bool peerSilent = receivedStart ? uint32_t(now - lastProxyPacketAt) >= 5000 :
                                          uint32_t(now - waitingSince) >= 5000;
    if (socketReady && peerSilent && uint32_t(now - lastRxRepairAt) >= 5000) {
        // NetworkUDP can stop reporting packets (including a zero-byte datagram
        // at the head of its queue). A quiet socket must never wedge recovery.
        // Do not discard the peer or stop input while reopening only the listener.
        Serial.println("[UDP] No control packets for 5s; reopening listener.");
        udp.stop();
        socketReady = false;
        lastSocketAttemptAt = now - socketRetryMs;
        lastRxRepairAt = now;
    }
    if (!receivedStart) return;
    const bool inputMissing = inputAckExpected && uint32_t(now - lastInputAckAt) >= 5000;
    if (inputMissing && !peerSilent && !inputLossNotified) {
        inputLossNotified = true;
        feedback.stopGame();
        feedback.notify(FeedbackEvent::ProxyLost, now);
        Serial.println("[UDP] Proxy heartbeat received, but input is not acknowledged; repairing TX.");
    }
    if (inputMissing && uint32_t(now - lastTxRepairAt) >= 5000) {
        inputUdp.stop();
        sendFailures = sendBackoffMs = 0;
        lastTxRepairAt = now;
    }
    const bool txStalled = txFailureActive && uint32_t(now - txFailureAt) >= 15000;
    if ((inputMissing && !peerSilent && uint32_t(now - lastInputAckAt) >= 15000) || txStalled) {
        Serial.printf("[WIFI] Input path still stalled after 15s; reconnecting station (RSSI=%d, heap=%u).\n",
                      WiFi.RSSI(), (unsigned int)ESP.getFreeHeap());
        stopOTA();
        closeSocket();
        wifiWasConnected = false;
        waitingSince = now;
        wifiSetup.retryConnection();
    }
}

void sendKeystrokes() {
    if (!receivedStart) return;
    const uint32_t now = millis();
    if (sendBackoffMs && uint32_t(now - sendBackoffAt) < sendBackoffMs) return;
    sendBackoffMs = 0;
    char packet[16];
    // Preserve the original 15-byte wire format and joystick axis order.
    const int length = snprintf(packet, sizeof(packet), "%c%d%d%d%d%d%04d%04d%d",
        controllerType[0], digitalRead(buttonA) == LOW, digitalRead(buttonB) == LOW,
        digitalRead(buttonSYS) == LOW, digitalRead(buttonTRIG) == LOW,
        digitalRead(buttonSQZ) == LOW, analogRead(joyXPin), analogRead(joyYPin),
        digitalRead(buttonJOYCLK) == LOW);
    bool sent = false;
    errno = 0;
    if (length == 15 && inputUdp.beginPacket(proxyIP, localPort)) {
        sent = inputUdp.write((const uint8_t*)packet, length) == size_t(length) && inputUdp.endPacket() == 1;
    }
    const int sendError = errno;
    if (sent) {
        sendFailures = 0;
        if (txLossNotified && !inputAckExpected) feedback.notify(FeedbackEvent::Connected, now);
        txFailureActive = txLossNotified = false;
    } else {
        if (!txFailureActive) { txFailureActive = true; txFailureAt = now; }
        if (!inputAckExpected && !txLossNotified && uint32_t(now - txFailureAt) >= 3000) {
            feedback.notify(FeedbackEvent::ProxyLost, now);
            txLossNotified = true;
        }
        ++sendFailures;
        if (!sendErrorLogged || uint32_t(now - lastSendErrorAt) >= 5000) {
            Serial.printf("[UDP] Input send failed: errno=%d, bytes=%d; retrying without disconnecting.\n", sendError, length);
            lastSendErrorAt = now;
            sendErrorLogged = true;
        }
        if (sendFailures >= 10) {
            // A full TX queue must not close the RX socket, lose START, or pulse
            // the motor repeatedly. Recreate only TX and let the network drain.
            inputUdp.stop();
            sendFailures = 0;
            sendBackoffAt = now;
            sendBackoffMs = 250;
        }
    }
}

void setup() {
    // Make the motor safe before starting serial, Wi-Fi or button setup.
    const bool motorReady = feedback.begin();
    Serial.begin(115200);
    if (!motorReady) Serial.println("[ERROR] Haptics disabled: PWM/timer safety setup failed.");
    rtc_gpio_deinit((gpio_num_t)buttonSYS);
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);
    pinMode(buttonSYS, INPUT_PULLUP);
    pinMode(buttonJOYCLK, INPUT_PULLUP);
    pinMode(buttonTRIG, INPUT_PULLUP);
    pinMode(buttonSQZ, INPUT_PULLUP);
    pinMode(joyXPin, INPUT);
    pinMode(joyYPin, INPUT);
    analogReadResolution(12);
    restartArmed = !(digitalRead(buttonSYS) == LOW && digitalRead(buttonTRIG) == LOW);

    configureWatchdog();
    Serial.printf("[SYSTEM] Controller %s; reset reason %d\n", controllerType, int(esp_reset_reason()));

    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    wifiSetup.begin(controllerType);
    configureOTA();
    feedback.notify(FeedbackEvent::Wake, millis());
    waitingSince = millis();
    // No blocking connection/START loops: buttons work even with the router off.
}

void loop() {
    serviceRestartGesture(millis());
    feedback.service(millis());
    if (restartPending) {
        if (!feedback.notificationActive()) {
            feedback.prepareStandby();
            ESP.restart();
        }
    } else if (sleepPending) {
        serviceSleep();
    } else {
        wifiSetup.service(millis());
        const bool portalActive = wifiSetup.portalActive();
        if (portalActive && !portalWasActive) feedback.notify(FeedbackEvent::Setup, millis());
        portalWasActive = portalActive;
        serviceNetwork(millis());
        serviceOTA();
        if (socketReady) receivePackets();
        const uint32_t now = millis();
        if (!sleepPending) serviceTransport(now);
        if (!sleepPending && receivedStart) {
            if (!proxyLossNotified && uint32_t(now - lastProxyPacketAt) >= 5000) {
                feedback.stopGame();
                feedback.notify(FeedbackEvent::ProxyLost, now);
                proxyLossNotified = true;
            }
            if (proxyLossNotified && uint32_t(now - lastProxyPacketAt) >= startTimeoutMs) requestSleep();
            if (!sleepPending && uint32_t(now - previousSendAt) >= sendInterval) {
                previousSendAt = now;
                sendKeystrokes();
            }
        } else if (!sleepPending && !wifiSetup.portalActive() && wifiWasConnected) {
            if (!proxyLossNotified && uint32_t(now - waitingSince) >= 5000) {
                feedback.notify(FeedbackEvent::ProxyLost, now);
                proxyLossNotified = true;
            }
            if (uint32_t(now - waitingSince) >= startTimeoutMs) requestSleep();
        }
    }
    feedback.service(millis());
    // Only feed after all loop work completed. A stuck network call must trip it.
    if (watchdogReady) esp_task_wdt_reset();
    delay(1); // Yield to Wi-Fi/RTOS without the old repeated 20/50/100 ms stalls.
}
