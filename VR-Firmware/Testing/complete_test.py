import socket
import json
import time
import websocket
import threading

# Define ports
pc_to_esp_port = 8888  # Port to send data from PC to ESP32
esp_to_pc_port = 9999  # Port to receive data from ESP32

# Broadcast IP address (using 255.255.255.255 for local network broadcast)
broadcast_ip = '255.255.255.255'

# Create UDP socket for sending and receiving
sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_send.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

sock_receive = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_receive.bind(('', esp_to_pc_port))  # Listen on the port where ESP32 sends data

# Function to send haptic feedback to ESP32
def send_to_esp32(duty_cycle, duration_ms):
    if duty_cycle == 0 and duration_ms == 0:
        return  # Avoid redundant stop signals

    try:
        # Format: XXXYYY (3 digits for amplitude, 3 digits for duration)
        formatted_data = f"{duty_cycle:03}{int(duration_ms):03}"
        sock_send.sendto(formatted_data.encode(), (broadcast_ip, pc_to_esp_port))  # Broadcast to ESP32
        print(f"[HAPTIC] Sent to right ESP32: {formatted_data}")
    except Exception as e:
        print(f"[HAPTIC ERROR] Right controller: {e}")

# WebSocket event handling functions
def on_message(ws, message):
    try:
        data = json.loads(message)
        event_data = data.get("event_type", {}).get("data", {})
        path = event_data.get("path", "")
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        if "/user/hand/right" in path:  # Focus on the right controller
            if duration and amplitude is not None:
                duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)
                duty_cycle = int(amplitude * 255)

                # Limit values to prevent formatting issues
                duty_cycle = max(0, min(255, duty_cycle))
                duration_ms = max(0, min(999, duration_ms))  # Max duration 999ms to fit 3 digits

                send_to_esp32(duty_cycle, duration_ms)
    except Exception as e:
        print(f"[ERROR] Processing WebSocket message: {e}")

def start_websocket():
    ws = websocket.WebSocketApp("ws://localhost:8082/api/events", on_message=on_message)
    ws.run_forever()

# Start WebSocket listener in a separate thread
ws_thread = threading.Thread(target=start_websocket)
ws_thread.daemon = True
ws_thread.start()

# Continuously listen for data from ESP32
try:
    while True:
        # Receive data from ESP32
        sock_receive.settimeout(0.00001)  # Wait for 1 second for response
        try:
            data, addr = sock_receive.recvfrom(1024)
            print(f"Received from ESP32: {data.decode()}")
        except socket.timeout:
            pass  # No response, continue loop

except KeyboardInterrupt:
    print("Exiting...")
finally:
    sock_send.close()
    sock_receive.close()
