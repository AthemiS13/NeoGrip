#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// UDP settings
WiFiUDP udp;
const unsigned int localPort = 9999;   // Port where ESP32 sends packets
const unsigned int hapticPort = 8888;  // Port to receive haptic data
const unsigned long sendInterval = 40; // 40ms interval for sending data

unsigned long previousMillis = 0;
char incomingPacket[255];  // Buffer to store incoming packets

const char *controllerType = "L";  // 'L' for left controller, 'R' for right controller

// GPIO Pins for buttons RIGHT
/*
const int buttonA = 13;
const int buttonB = 12;
const int buttonSYS = 14;
const int buttonJOYCLK = 35;
const int buttonTRIG = 18;
const int buttonSQZ = 19;
const int joyXPin = 33;
const int joyYPin = 32;
*/ 

// GPIO Pins for buttons LEFT
const int buttonA = 19;
const int buttonB = 12;
const int buttonSYS = 21;
const int buttonJOYCLK =4;
const int buttonTRIG = 17;
const int buttonSQZ = 16;
const int joyXPin = 15;
const int joyYPin = 2;
// PWM settings for haptic feedback
const int motorPin = 16;
const int pwmResolution = 8;
const int fixedFrequency = 200;
const int maxDuty = 0;
const int minDuty = 255;

// Timing control for haptic motor without delay
unsigned long motorEndTime = 0;

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        // Wait until connected (still no delay in this loop)
    }
    Serial.println("Connected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    udp.begin(localPort);
    udp.begin(hapticPort);// Start listening for haptic data
    Serial.println("UDP server started");

    // Configure GPIO pins
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);
    pinMode(buttonSYS, INPUT_PULLUP);
    pinMode(buttonJOYCLK, INPUT);
    pinMode(buttonTRIG, INPUT_PULLUP);
    pinMode(buttonSQZ, INPUT_PULLUP);
    pinMode(joyXPin, INPUT);
    pinMode(joyYPin, INPUT);

    // Configure PWM for motor
    ledcAttach(motorPin, fixedFrequency, pwmResolution);
    ledcWrite(motorPin, maxDuty);  // Ensure motor is off initially
}

void loop() {
    unsigned long currentMillis = millis();

    // Handle keystrokes
    if (currentMillis - previousMillis >= sendInterval) {
        previousMillis = currentMillis;
        sendKeystrokes();
    }

    // Handle haptic feedback
    receiveHaptics(currentMillis);
}

void sendKeystrokes() {
    int buttonAState = digitalRead(buttonA) == LOW ? 1 : 0;
    int buttonBState = digitalRead(buttonB) == LOW ? 1 : 0;
    int buttonSYSState = digitalRead(buttonSYS) == LOW ? 1 : 0;
    int buttonJOYCLKState = digitalRead(buttonJOYCLK) == LOW ? 1 : 0;
    int buttonTRIGState = digitalRead(buttonTRIG) == LOW ? 1 : 0;
    int buttonSQZState = digitalRead(buttonSQZ) == LOW ? 1 : 0;

    int joyXValue = analogRead(joyXPin);  // Uncomment if joystick values are needed
    int joyYValue = analogRead(joyYPin);

    String packet = String(controllerType) + String(buttonAState) + String(buttonBState) + 
                    String(buttonSYSState) + String(buttonTRIGState) + 
                    String(buttonSQZState) + String(joyXValue) + 
                    String(joyYValue) + String(buttonJOYCLKState);
    
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
        Serial.printf("Received packet: %s\n", incomingPacket);
        
        // Ensure the packet starts with the correct controller identifier
        if (incomingPacket[0] == controllerType[0]) {
            int dutyCycle, duration;
            if (sscanf(incomingPacket + 1, "%3d%3d", &dutyCycle, &duration) == 2) {
                dutyCycle = constrain(dutyCycle, minDuty, maxDuty);
                ledcWrite(motorPin, dutyCycle);
                Serial.printf("Motor ON - Duty Cycle: %d\n", dutyCycle);
                
                // Start timer for motor duration
                motorEndTime = currentMillis + duration;
            }
        }
    }

    // Turn off motor after duration
    if (currentMillis >= motorEndTime) {
        delay(20);
        ledcWrite(motorPin, maxDuty);  // Turn off motor after duration ends
        Serial.println("Motor OFF");
    }
}
