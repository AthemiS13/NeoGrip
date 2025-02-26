#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// UDP settings
WiFiUDP udp;
const unsigned int localPort = 8888;
char incomingPacket[255];

// PWM settings for the vibration motor
const int motorPin = 16;
const int pwmResolution = 8;  // 8-bit resolution (0-255)
const int fixedFrequency = 200; // Fixed frequency in Hz
const int maxDuty = 255;   // Motor OFF
const int minDuty = 0;     // Motor ON at max power

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Start UDP server
    udp.begin(localPort);
    Serial.printf("Listening on port %d\n", localPort);
    delay(2000);

    // Configure PWM for motor
    ledcAttach(motorPin, fixedFrequency, pwmResolution);
    ledcWrite(motorPin, maxDuty); // Ensure motor starts off
}

void loop() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
        if (len > 0) {
            incomingPacket[len] = '\0';
        }
        Serial.printf("Received packet: %s\n", incomingPacket);
        
        // Parse received data (duty cycle 0-255, duration in ms)
        int dutyCycle, duration;
        if (sscanf(incomingPacket, "%d,%d", &dutyCycle, &duration) == 2) {
            Serial.printf("Parsed values - Duty Cycle: %d, Duration: %d ms\n", dutyCycle, duration);

            // Clamp duty cycle within valid range (0-255)
            dutyCycle = constrain(dutyCycle, minDuty, maxDuty);

            // Apply PWM duty cycle
            ledcWrite(motorPin, dutyCycle);
            Serial.printf("Motor ON - Duty Cycle: %d\n", dutyCycle);

            // Maintain vibration for the specified duration
            delay(duration);



            // Stop motor after duration
            ledcWrite(motorPin, maxDuty);
            Serial.println("Motor OFF");
        } else {
            Serial.println("Error parsing packet");
            ledcWrite(motorPin, maxDuty);
            Serial.println("Motor OFF");
        }
    }
}
