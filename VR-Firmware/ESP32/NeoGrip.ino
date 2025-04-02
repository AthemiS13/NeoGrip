#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_sleep.h"

// WiFi credentials
const char* ssid = ""; // Your WiFi SSID
const char* password = ""; // Your WiFi password

// UDP settings
WiFiUDP udp;
const unsigned int localPort = 9999;   // Port where ESP32 sends packets
const unsigned int hapticPort = 8888;  // Port to receive haptic data
const unsigned long sendInterval = 40; // 40ms interval for sending data

unsigned long previousMillis = 0;
char incomingPacket[255];  // Buffer to store incoming packets

const char *controllerType = "R";  // 'L' for left controller, 'R' for right controller

// GPIO Pins for buttons RIGHT
const int buttonA = 21;
const int buttonB = 18;
const int buttonSYS = 4;  // Also used as wakeup button
const int buttonJOYCLK = 14;
const int buttonTRIG = 19;
const int buttonSQZ = 27;
const int joyXPin = 34;
const int joyYPin = 35;

// PWM settings for haptic feedback
const int motorPin = 16;
const int pwmResolution = 8;
const int fixedFrequency = 200;
const int maxDuty = 255; // Full Speed
const int minDuty = 0; // OFF

// Timing control for haptic motor without delay
unsigned long motorEndTime = 0;

// Flags to track START/STOP detection
bool receivedStart = false;

void goToDeepSleep() {
    Serial.println("[SYSTEM] Going to deep sleep...");

    // Configure wake-up source (GPIO 4 - buttonSYS)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonSYS, LOW);
    hapticSignal();

    // Enter deep sleep
    esp_deep_sleep_start();

}

void waitForStart() {
    Serial.println("[WAIT] Waiting for START signal...");

    unsigned long startTime = millis();  // Record wake-up time
    const unsigned long timeout = 60000; // 60 seconds timeout

    while (!receivedStart) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
            int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
            if (len > 0) {
                incomingPacket[len] = '\0';
            }
            Serial.printf("[UDP] Received: %s\n", incomingPacket);

            if (strcmp(incomingPacket, "START") == 0) {
                receivedStart = true;
                Serial.println("[SYSTEM] START received! Proceeding...");
                hapticSignal();
                return;  // Exit function and continue normal operation
            }
        }

        // Check if 30 seconds have passed
        if (millis() - startTime >= timeout) {
            Serial.println("[TIMEOUT] No START received. Going back to deep sleep...");
            goToDeepSleep();
        }

        delay(50);  // Small delay to prevent busy-waiting
    }
}

void setup() {
    Serial.begin(115200); 

    // Connect to WiFi
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);

                // Restart ESP if it fails to connect within 10 seconds
        if (millis() - startAttemptTime >= 10000) {
            Serial.println("\n[ERROR] Failed to connect to WiFi! Restarting ESP...");
            ESP.restart();
        }
    }
    Serial.println("\n[SYSTEM] Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // Start UDP listening
    udp.begin(localPort);
    udp.begin(hapticPort);  // Listen for haptic data
    Serial.println("[SYSTEM] UDP server started");

    // Configure GPIO pins
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);
    pinMode(buttonSYS, INPUT_PULLUP);  // Used as wake-up button
    pinMode(buttonJOYCLK, INPUT_PULLUP);
    pinMode(buttonTRIG, INPUT_PULLUP);
    pinMode(buttonSQZ, INPUT_PULLUP);
    pinMode(joyXPin, INPUT);
    pinMode(joyYPin, INPUT);


    
    // Configure PWM for motor
    ledcAttach(motorPin, fixedFrequency, pwmResolution);
    hapticSignal();
    ledcWrite(motorPin, maxDuty);  // Ensure motor is off initially

    
    // Wait for START before proceeding
   waitForStart();
}

void loop() {
    unsigned long currentMillis = millis();


    // Handle keystrokes
    if (currentMillis - previousMillis >= sendInterval) {
        previousMillis = currentMillis;
        sendKeystrokes();
    }

    // Handle haptic feedback and check for STOP signal
    receiveHaptics(currentMillis);
}

void sendKeystrokes() {
    int buttonAState = digitalRead(buttonA) == LOW ? 1 : 0;
    int buttonBState = digitalRead(buttonB) == LOW ? 1 : 0;
    int buttonSYSState = digitalRead(buttonSYS) == LOW ? 1 : 0;
    int buttonJOYCLKState = digitalRead(buttonJOYCLK) == LOW ? 1 : 0;
    int buttonTRIGState = digitalRead(buttonTRIG) == LOW ? 1 : 0;
    int buttonSQZState = digitalRead(buttonSQZ) == LOW ? 1 : 0;

    int joyXValue = analogRead(joyXPin);
    int joyYValue = analogRead(joyYPin);

    char joyXFormatted[5];  // Buffer for formatted joystick X value
    char joyYFormatted[5];  // Buffer for formatted joystick Y value
    sprintf(joyXFormatted, "%04d", joyXValue);
    sprintf(joyYFormatted, "%04d", joyYValue);

    String packet = String(controllerType) + String(buttonAState) + String(buttonBState) +
                    String(buttonSYSState) + String(buttonTRIGState) +
                    String(buttonSQZState) + String(joyXFormatted) +
                    String(joyYFormatted) + String(buttonJOYCLKState);
    
    udp.beginPacket("255.255.255.255", localPort);  // Broadcast to all devices
    udp.print(packet);
    udp.endPacket();
    Serial.println("Sent: " + packet);
}

void receiveHaptics(unsigned long currentMillis) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
        if (len > 0) {
            incomingPacket[len] = '\0';
        }
        Serial.printf("[UDP] Received: %s\n", incomingPacket);

        // Check for STOP signal
        if (strcmp(incomingPacket, "STOP") == 0) {
            Serial.println("[SYSTEM] STOP received! Entering deep sleep...");
            goToDeepSleep();
        }

        // Ensure the packet starts with the correct controller identifier
        if (incomingPacket[0] == controllerType[0]) {
            int dutyCycle, duration;
            if (sscanf(incomingPacket + 1, "%3d%3d", &dutyCycle, &duration) == 2) {
                dutyCycle = constrain(dutyCycle, minDuty, maxDuty);
                ledcWrite(motorPin, dutyCycle);
                Serial.printf("[HAPTIC] Motor ON - Duty Cycle: %d\n", dutyCycle);

                // Start timer for motor duration
                motorEndTime = currentMillis + duration;
            }
        }
    }

    // Turn off motor after duration
    if (currentMillis >= motorEndTime) {
        delay(20);
        ledcWrite(motorPin, maxDuty);  // Turn off motor after duration ends
    }
}
void hapticSignal() {
    // Turn motor on
    ledcWrite(motorPin,15);  


    delay(100);

    // Turn motor off
    ledcWrite(motorPin, minDuty);
    delay(100);
}
