import socket
import threading
import requests
import time
import json
import websocket
from queue import Queue

# ALVR Server configuration
ALVR_URL = "http://192.168.0.28:8082/api/set-buttons"
SEND_INTERVAL = 0.04  # 40ms between updates to ALVR
BUFFER_SIZE = 1024  # UDP buffer size

# Ports for controllers
CONTROLLER_PORTS = {"left": 8888, "right": 8889}

# Shared state
state_queue = Queue()
last_sent_data = {"left": None, "right": None}

# ESP32 Haptic Configuration
ESP32_IP = "192.168.0.35"  # Change to your ESP32 IP
ESP32_PORTS = {"left": 8890, "right": 8891}  # Different ports for each controller

# UDP socket for sending haptic feedback
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Function to send button states and joystick values to ALVR server
def send_to_alvr(controller, data):
    try:
        response = requests.post(ALVR_URL, json=data)
        if response.status_code != 200:
            print(f"ALVR Error [{controller.upper()}]: {response.status_code} - {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"ALVR Request Error [{controller.upper()}]: {e}")

# Packet processing function
def process_packet(controller, packet):
    try:
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

        # Add the data to the queue
        state_queue.put((controller, data))
    except ValueError as e:
        print(f"Packet decode error for {controller}: {e}")

# Listener function for a specific controller
def listen_for_controller(controller, port):
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind(('0.0.0.0', port))
    print(f"Listening for {controller} controller on port {port}...")

    while True:
        try:
            data, addr = udp_socket.recvfrom(BUFFER_SIZE)
            packet = data.decode()
            if len(packet) == 15:  # Ensure valid packet length
                process_packet(controller, packet[1:])  # Skip controller identifier
            else:
                print(f"Invalid packet length from {addr}: {len(packet)}")
        except Exception as e:
            print(f"Error in {controller} listener: {e}")
        except KeyboardInterrupt:
            print(f"Stopping {controller} listener.")
            break
    udp_socket.close()

# Function to handle sending updates to ALVR
def update_alvr():
    while True:
        try:
            if not state_queue.empty():
                controller, data = state_queue.get()
                if data != last_sent_data[controller]:
                    send_to_alvr(controller, data)
                    last_sent_data[controller] = data
            time.sleep(SEND_INTERVAL)
        except KeyboardInterrupt:
            print("Stopping ALVR updater.")
            break

# Function to send haptic feedback to ESP32
def send_to_esp32(controller, duty_cycle, duration_ms):
    try:
        formatted_data = f"{duty_cycle},{int(duration_ms)}"
        sock.sendto(formatted_data.encode(), (ESP32_IP, ESP32_PORTS[controller]))
        print(f"Sent to {controller} ESP32: {formatted_data}")
    except Exception as e:
        print(f"Error sending data to {controller} ESP32: {e}")

# WebSocket event handling functions
def on_message(ws, message):
    """Handles incoming WebSocket messages from ALVR."""
    try:
        data = json.loads(message)
        event_data = data.get("event_type", {}).get("data", {})
        path = event_data.get("path", "")
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        # Determine if it's left or right hand based on the path
        if "/user/hand/left" in path:
            controller = "left"
        elif "/user/hand/right" in path:
            controller = "right"
        else:
            return  # Ignore unknown paths

        # Ensure required fields exist
        if duration and amplitude is not None:
            duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)
            duty_cycle = int(amplitude * 255)
            send_to_esp32(controller, duty_cycle, duration_ms)
        else:
            send_to_esp32(controller, 255, 0)  # Stop vibration

    except json.JSONDecodeError:
        print("Failed to decode JSON message")
    except Exception as e:
        print(f"Error processing message: {e}")

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
    ws = websocket.WebSocketApp("ws://localhost:8082/api/events",
                                on_message=on_message,
                                on_error=on_error,
                                on_close=on_close)
    ws.on_open = on_open
    ws.run_forever()

# Main function
def main():
    try:
        listener_threads = []
        for controller, port in CONTROLLER_PORTS.items():
            thread = threading.Thread(target=listen_for_controller, args=(controller, port))
            thread.daemon = True
            listener_threads.append(thread)
            thread.start()

        updater_thread = threading.Thread(target=update_alvr)
        updater_thread.daemon = True
        updater_thread.start()

        websocket_thread = threading.Thread(target=start_websocket)
        websocket_thread.daemon = True
        websocket_thread.start()

        print("Proxy server running. Press Ctrl+C to stop.")
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("Stopping proxy server...")
    finally:
        for thread in listener_threads:
            thread.join(timeout=1)

if __name__ == "__main__":
    main()
