#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";
const char* server_url = "http://192.168.0.28:8082/api/set-buttons"; // Your PC's IP


// Button pin assignments (Right Controller)
const int pin_A = 27;
const int pin_B = 14;
const int pin_trigger = 16;
const int pin_grab = 17;
const int pin_menu = 5;

// Variables to hold button states
bool last_A_state = false;
bool last_B_state = false;
bool last_trigger_state = false;
bool last_grab_state = false;
bool last_menu_state = false;

void setup() {
  // Start Serial Monitor
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Setup button pins
  pinMode(pin_A, INPUT_PULLUP);
  pinMode(pin_B, INPUT_PULLUP);
  pinMode(pin_trigger, INPUT_PULLUP);
  pinMode(pin_grab, INPUT_PULLUP);
  pinMode(pin_menu, INPUT_PULLUP);
}

// Function to send button state to ALVR
void sendButtonStates(const char* path, bool state, bool isScalar = false) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(server_url);
    http.addHeader("Content-Type", "application/json");

    // Create JSON array payload
    DynamicJsonDocument doc(1024);
    JsonArray root = doc.to<JsonArray>();

    // Add the button state object
    JsonObject buttonState = root.createNestedObject();
    buttonState["path"] = path;
    JsonObject value = buttonState.createNestedObject("value");

    // Use Scalar for trigger/grab, Binary for others
    if (isScalar) {
      value["Scalar"] = state ? 1.0 : 0.0;
    } else {
      value["Binary"] = state;
    }

    // Serialize the JSON array
    String json;
    serializeJson(root, json);
    Serial.println(json); // Debug: print payload

    // Send POST request
    int httpResponseCode = http.POST(json);
    if (httpResponseCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("Error code: %d\n", httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void loop() {
  // Read button states
  bool current_A_state = digitalRead(pin_A) == LOW;
  bool current_B_state = digitalRead(pin_B) == LOW;
  bool current_trigger_state = digitalRead(pin_trigger) == LOW;
  bool current_grab_state = digitalRead(pin_grab) == LOW;
  bool current_menu_state = digitalRead(pin_menu) == LOW;

  // Check if button states have changed and send the new state
  if (current_A_state != last_A_state) {
    sendButtonStates("/user/hand/right/input/a/click", current_A_state);
    last_A_state = current_A_state;
  }

  if (current_B_state != last_B_state) {
    sendButtonStates("/user/hand/right/input/b/click", current_B_state);
    last_B_state = current_B_state;
  }

  if (current_trigger_state != last_trigger_state) {
    sendButtonStates("/user/hand/right/input/trigger/value", current_trigger_state, true);  // Scalar for trigger
    last_trigger_state = current_trigger_state;
  }

  if (current_grab_state != last_grab_state) {
    sendButtonStates("/user/hand/right/input/squeeze/value", current_grab_state, true);  // Scalar for grab
    last_grab_state = current_grab_state;
  }

  if (current_menu_state != last_menu_state) {
    sendButtonStates("/user/hand/right/input/menu/click", current_menu_state);
    last_menu_state = current_menu_state;
  }

  delay(100);  // Adjust delay as needed to avoid flooding the server
}
