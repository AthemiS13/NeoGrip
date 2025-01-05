#include <WiFi.h>
#include <HTTPClient.h>

// Define your Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// Define the server URL
const String serverURL = "http://192.168.0.28:8082/api/set-buttons";

// Define GPIO pins for trigger and grab buttons
const int triggerPin = 16;
const int grabPin = 17;

// Function to send button states to the server
void sendButtonStates(float triggerValue, float grabValue) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverURL); // Specify the server URL
    http.addHeader("Content-Type", "application/json");

    // Create the JSON payload
    String payload = "["
                        "{\"path\": \"/user/hand/right/input/trigger/value\", \"value\": {\"Scalar\": " + String(triggerValue) + "}},"
                        "{\"path\": \"/user/hand/right/input/squeeze/value\", \"value\": {\"Scalar\": " + String(grabValue) + "}}"
                     "]";

    // Send the POST request
    int httpCode = http.POST(payload);
    if (httpCode == 200) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Failed to send data. HTTP Code: " + String(httpCode));
    }

    http.end();  // Close the connection
  } else {
    Serial.println("Wi-Fi not connected!");
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Wi-Fi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Set button pins as input
  pinMode(triggerPin, INPUT_PULLUP); // Trigger button (GPIO 16)
  pinMode(grabPin, INPUT_PULLUP);    // Grab button (GPIO 17)
}

void loop() {
  // Read button states (active low, so LOW means pressed)
  float triggerState = (digitalRead(triggerPin) == LOW) ? 0.8 : 0.0;
  float grabState = (digitalRead(grabPin) == LOW) ? 1.0 : 0.0;

  // Send the states to the server
  sendButtonStates(triggerState, grabState);

  // Wait for 10 ms before updating again
  delay(50);
}
