import socket
import threading
import requests
import time
import json
import websocket
from queue import Queue

# ALVR Server configuration
ALVR_URL = "http://192.168.0.30:8082/api/set-buttons"
SEND_INTERVAL = 0  # Reduced to 20ms for better responsiveness
BUFFER_SIZE = 1024  # UDP buffer size

# Ports for controllers
CONTROLLER_PORTS = {"left": 8888, "right": 8889}

# Shared state
state_queue = Queue()
last_sent_data = {"left": None, "right": None}

# ESP32 Haptic Configuration
ESP32_IP = "192.168.0.21"  # Change to your ESP32 IP
ESP32_PORTS = {"left": 8888, "right": 8891}  # Different ports for each controller

# UDP socket for sending haptic feedback
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Function to send button states and joystick values to ALVR server
def send_to_alvr(controller, data):
    print(f"[ALVR] Sending to ALVR ({controller}): {json.dumps(data, indent=2)}")
    try:
        response = requests.post(ALVR_URL, json=data)
        if response.status_code != 200:
            print(f"[ALVR ERROR] {controller.upper()} - {response.status_code}: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"[ALVR ERROR] {controller.upper()} - {e}")

# Packet processing function
def process_packet(controller, packet):
    try:
        print(f"[ESP] Raw data from {controller}: {packet}")
        if len(packet) != 14:
            print(f"[ERROR] Invalid packet length: {len(packet)}")
            return

        # Decode the packet
        button_primary = int(packet[0])
        button_secondary = int(packet[1])
        button_menu_or_sys = int(packet[2])
        button_TRIG = int(packet[3])
        button_SQZ = int(packet[4])
        joyX = int(packet[5:9])
        joyY = int(packet[9:13])
        button_JOYCLK = int(packet[13])

        # Convert to ALVR-compatible format
        data = [
            {"path": f"/user/hand/{controller}/input/trigger/value", "value": {"Scalar": 0.8 if button_TRIG else 0.0}},
            {"path": f"/user/hand/{controller}/input/squeeze/value", "value": {"Scalar": 1.0 if button_SQZ else 0.0}},
            {"path": f"/user/hand/{controller}/input/{'x' if controller == 'left' else 'a'}/click", "value": {"Binary": button_primary == 1}},
            {"path": f"/user/hand/{controller}/input/{'y' if controller == 'left' else 'b'}/click", "value": {"Binary": button_secondary == 1}},
            {"path": f"/user/hand/{controller}/input/{'menu' if controller == 'left' else 'system'}/click", "value": {"Binary": button_menu_or_sys == 1}},
            {"path": f"/user/hand/{controller}/input/thumbstick/x", "value": {"Scalar": (joyX - 2047) / 2047.0}},
            {"path": f"/user/hand/{controller}/input/thumbstick/y", "value": {"Scalar": (joyY - 2047) / 2047.0}},
            {"path": f"/user/hand/{controller}/input/thumbstick/click", "value": {"Binary": button_JOYCLK == 1}},
        ]
        
        state_queue.put((controller, data))
    except ValueError as e:
        print(f"[ERROR] Packet decode error for {controller}: {e}")

# Function to handle sending updates to ALVR
def update_alvr():
    while True:
        if not state_queue.empty():
            controller, data = state_queue.get()
            if data != last_sent_data[controller]:
                send_to_alvr(controller, data)
                last_sent_data[controller] = data
        time.sleep(SEND_INTERVAL)

# Function to send haptic feedback to ESP32
def send_to_esp32(controller, duty_cycle, duration_ms):
    if duty_cycle == 0 and duration_ms == 0:
        return  # Avoid redundant stop signals
    try:
        formatted_data = f"{duty_cycle},{int(duration_ms)}"
        sock.sendto(formatted_data.encode(), (ESP32_IP, ESP32_PORTS[controller]))
        print(f"[HAPTIC] Sent to {controller} ESP32: {formatted_data}")
    except Exception as e:
        print(f"[HAPTIC ERROR] {controller}: {e}")

# WebSocket event handling functions
def on_message(ws, message):
    try:
        data = json.loads(message)
        event_data = data.get("event_type", {}).get("data", {})
        path = event_data.get("path", "")
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        if "/user/hand/left" in path:
            controller = "left"
        elif "/user/hand/right" in path:
            controller = "right"
        else:
            return

        if duration and amplitude is not None:
            duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)
            duty_cycle = int(amplitude * 255)
            send_to_esp32(controller, duty_cycle, duration_ms)
    except Exception as e:
        print(f"[ERROR] Processing WebSocket message: {e}")

def start_websocket():
    ws = websocket.WebSocketApp("ws://localhost:8082/api/events", on_message=on_message)
    ws.run_forever()
# Listener function for a specific controller
def listen_for_controller(controller, port):
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(('0.0.0.0', port))
    print(f"Listening for {controller} controller on port {port}...")

    while True:
        try:
            data, addr = udp_socket.recvfrom(BUFFER_SIZE)
            packet = data.decode()
            print(f"Received from {controller} ({addr}): {packet}")

            if len(packet) == 15:  # Ensure valid packet length
                process_packet(controller, packet[1:])  # Skip controller identifier
            else:
                print(f"Invalid packet length from {addr}: {len(packet)}")
        except Exception as e:
            print(f"Error in {controller} listener: {e}")
            break
    udp_socket.close()

def main():
    try:
        threading.Thread(target=update_alvr, daemon=True).start()
        threading.Thread(target=start_websocket, daemon=True).start()
        for controller, port in CONTROLLER_PORTS.items():
            threading.Thread(target=listen_for_controller, args=(controller, port), daemon=True).start()
        print("[SYSTEM] Proxy server running. Press Ctrl+C to stop.")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[SYSTEM] Stopping proxy server...")

if __name__ == "__main__":
    main()
