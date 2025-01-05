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

// GPIO Pins for buttons
const int buttonA = 18;
const int buttonB = 5;
const int buttonSYS = 19;
const int buttonJOYCLK = 34;  // Assuming the joystick click button is on GPIO 34
const int buttonTRIG = 16;
const int buttonSQZ = 4;
const int joyXPin = 32;  // X-axis of the joystick
const int joyYPin = 35;  // Y-axis of the joystick

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
  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  pinMode(buttonSYS, INPUT_PULLUP);
  pinMode(buttonJOYCLK, INPUT_PULLUP);  // Change to INPUT (no pull-up resistor) for joystick click
  pinMode(buttonTRIG, INPUT_PULLUP);
  pinMode(buttonSQZ, INPUT_PULLUP);
  pinMode(joyXPin, INPUT);
  pinMode(joyYPin, INPUT);

  Serial.println("Setup complete!");
}

void loop() {
  unsigned long currentMillis = millis();

  // Read button states (buttons use pull-up, so LOW means pressed)
  int buttonAState = digitalRead(buttonA) == LOW ? 1 : 0;
  int buttonBState = digitalRead(buttonB) == LOW ? 1 : 0;
  int buttonSYSState = digitalRead(buttonSYS) == LOW ? 1 : 0;
  int buttonJOYCLKState = digitalRead(buttonJOYCLK) == LOW ? 1 : 0;  // Joystick click should now be detected correctly
  int buttonTRIGState = digitalRead(buttonTRIG) == LOW ? 1 : 0;
  int buttonSQZState = digitalRead(buttonSQZ) == LOW ? 1 : 0;

  // Read joystick values (range: 0 to 1023)
  int joyXValue = analogRead(joyXPin);
  int joyYValue = analogRead(joyYPin);

  // Format joystick values as 4-digit strings (range 0000 to 4095)
  String formattedJoyX = String(joyXValue);
  String formattedJoyY = String(joyYValue);
  formattedJoyX = formattedJoyX.length() < 4 ? String("0000" + formattedJoyX).substring(formattedJoyX.length()) : formattedJoyX;
  formattedJoyY = formattedJoyY.length() < 4 ? String("0000" + formattedJoyY).substring(formattedJoyY.length()) : formattedJoyY;

  // Format the packet with all button and joystick states
  String packet = String(buttonAState) + String(buttonBState) + String(buttonSYSState) +
                  String(buttonTRIGState) + String(buttonSQZState) + 
                  formattedJoyX + formattedJoyY + String(buttonJOYCLKState);  // Send formatted joystick values

  // Check if it's time to send a new packet
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Send packet to the proxy server
    udp.beginPacket(proxy_ip, proxy_port);
    udp.print(packet);  // Send the data as a string
    udp.endPacket();

    // Debug: Print confirmation of sending
    Serial.println("Sent: " + packet);
  }
}
