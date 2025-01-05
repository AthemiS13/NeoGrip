#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "";
const char* password = "";

WiFiUDP udp;
const char* proxy_ip = "192.168.0.28";  // IP of your Python proxy server
const int proxy_port = 8888;
const unsigned long interval = 40;  // 40ms interval for sending data

unsigned long previousMillis = 0;  // Will store the last time a packet was sent

void setup() {
  // Start Serial for debugging
  Serial.begin(115200);

  // Set up Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");

  // Begin UDP communication
  udp.begin(8888);

  // Configure GPIO pins for buttons
  pinMode(16, INPUT_PULLUP);  // Use internal pull-up resistor
  pinMode(17, INPUT_PULLUP);  // Use internal pull-up resistor

  Serial.println("Setup complete!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Read button states
  int button16_raw = digitalRead(16);  // Raw state of button 16
  int button17_raw = digitalRead(17);  // Raw state of button 17

  // Reverse logic: HIGH (not pressed) -> 0, LOW (pressed) -> 1
  int trigger_value = button16_raw == LOW ? 1 : 0;
  int squeeze_value = button17_raw == LOW ? 1 : 0;

  // Format the packet as "00", "01", "10", or "11"
  String packet = String(trigger_value) + String(squeeze_value);


  // Check if it's time to send a new packet
  if (currentMillis - previousMillis >= interval) {
    // Save the last time we sent a packet
    previousMillis = currentMillis;

    // Send packet to the proxy server
    udp.beginPacket(proxy_ip, proxy_port);
    udp.print(packet);  // Send the data as a string
    udp.endPacket();

    // Debug: Print confirmation of sending
    Serial.println("Sent: " + packet);

  }
}
