import websocket
import json
import socket
import time

# WebSocket URL to connect to ALVR server
WS_URL = "ws://localhost:8082/api/events"

# ESP32 UDP Address and Port
ESP32_IP = "192.168.0.35"  # Change to your ESP32 IP
ESP32_PORT = 8888

# UDP socket for sending data to ESP32
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def on_message(ws, message):
    """Handles incoming WebSocket messages from ALVR."""
    try:
        # Parse incoming JSON message
        data = json.loads(message)

        # Extract vibration event fields
        event_data = data.get("event_type", {}).get("data", {})
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        # Ensure required fields exist
        if duration and amplitude is not None:
            # Convert duration to milliseconds
            duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)

            # Convert amplitude from 0.0 - 1.0 to 0 - 255
            duty_cycle = int(amplitude * 255)

            # Format data as "duty_cycle,duration"
            formatted_data = f"{duty_cycle},{int(duration_ms)}"

            # Send data to ESP32
            send_to_esp32(formatted_data)
        else:
            print("Error: Missing required fields (duration, amplitude)")

    except json.JSONDecodeError:
        print("Failed to decode JSON message")
    except Exception as e:
        print(f"Error processing message: {e}")

def send_to_esp32(data):
    """Sends vibration data to ESP32 over UDP."""
    try:
        sock.sendto(data.encode(), (ESP32_IP, ESP32_PORT))
        print(f"Sent to ESP32: {data}")
    except Exception as e:
        print(f"Error sending data to ESP32: {e}")

def on_error(ws, error):
    """Handles WebSocket errors."""
    print(f"WebSocket Error: {error}")

def on_close(ws, close_status_code, close_msg):
    """Handles WebSocket disconnections."""
    print("WebSocket connection closed. Reconnecting in 5 seconds...")
    time.sleep(5)
    start_websocket()  # Reconnect

def on_open(ws):
    """Handles WebSocket connection opening."""
    print("Connected to WebSocket server")

def start_websocket():
    """Starts the WebSocket client."""
    ws = websocket.WebSocketApp(WS_URL, on_message=on_message, on_error=on_error, on_close=on_close)
    ws.on_open = on_open
    ws.run_forever()

# Start the WebSocket connection
start_websocket()
