import socket
import json
import requests
import threading
import queue
import websocket
import time

# ESP32 to PC communication
esp_to_pc_port = 9999  # Port where ESP32 sends packets
pc_to_esp_port = 8888  # Port to send haptic feedback to ESP32
alvr_url = "http://127.0.0.1:8082/api/set-buttons"
broadcast_ip = "255.255.255.255"  # Broadcast IP address for local network
#target_ip = '192.168.0.25'

# Create UDP sockets
sock_receive = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_receive.bind(('', esp_to_pc_port))  # Listen for packets from ESP32

sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_send.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

# Queue to process controller states
state_queue = queue.Queue()

# Flag to check if controller has responded
controller_connected = False

# Function to send initialization/shutdown messages
def send_startup_signal():
    print("[INIT] Press system button on controller to connect")
    global controller_connected
    while not controller_connected:
        try:
            sock_send.sendto(b"START", (broadcast_ip, pc_to_esp_port))
            print(".", end="", flush=True)   
            time.sleep(0.5)
        except Exception as e:
            print(f"[ERROR] Failed to send START: {e}")


# Packet processing function
def process_packet(packet):
    global controller_connected
    try:
        print(f"[ESP] Raw data: {packet}")

        if len(packet) != 15:
            print(f"[ERROR] Invalid packet length: {len(packet)}")
            return

        controller_connected = True  # Mark as connected once we receive data

        controller_id = packet[0]  # 'L' or 'R'
        button_1 = int(packet[1])  # A/X
        button_2 = int(packet[2])  # B/Y
        button_SYS = int(packet[3])
        button_TRIG = int(packet[4])
        button_SQZ = int(packet[5])

        # Properly parse joystick X and Y values from string
        joyY = int(packet[6:10])  # Extract substring and convert
        joyX = int(packet[10:14])

        button_JOYCLK = int(packet[14])

        # Determine the path prefix based on controller ID
        hand_path = "/user/hand/left" if controller_id == 'L' else "/user/hand/right"

        # Adjust button names based on controller side
        if controller_id == 'L':
            btn1_name = "x"
            btn2_name = "y"
        else:
            btn1_name = "a"
            btn2_name = "b"

        # Convert to ALVR-compatible format
        data = [
            {"path": f"{hand_path}/input/trigger/value", "value": {"Scalar": 0.8 if button_TRIG else 0.0}},
            {"path": f"{hand_path}/input/squeeze/value", "value": {"Scalar": 1.0 if button_SQZ else 0.0}},
            {"path": f"{hand_path}/input/{btn1_name}/click", "value": {"Binary": button_1 == 1}},
            {"path": f"{hand_path}/input/{btn2_name}/click", "value": {"Binary": button_2 == 1}},
            {"path": f"{hand_path}/input/system/click", "value": {"Binary": button_SYS == 1}},
            {"path": f"{hand_path}/input/thumbstick/x", "value": {"Scalar": (joyY - 2047) / 2047.0 if controller_id == 'L' else (joyX - 2047) / 2047.0}},
            {"path": f"{hand_path}/input/thumbstick/y", "value": {"Scalar": (joyX - 2047) / 2047.0 if controller_id == 'L' else (joyY - 2047) / 2047.0}},
            {"path": f"{hand_path}/input/thumbstick/click", "value": {"Binary": button_JOYCLK == 1}},
        ]

        state_queue.put(data)
    except ValueError as e:
        print(f"[ERROR] Packet decode error: {e}")

# Function to send data to ALVR
def send_to_alvr():
    while True:
        try:
            data = state_queue.get()
            response = requests.post(alvr_url, json=data)
        except Exception as e:
            print(f"[ERROR] ALVR request failed: {e}")

# Function to send haptic feedback to ESP32
def send_to_esp32(controller_id, duty_cycle, duration_ms):
    if duty_cycle == 0 and duration_ms == 0:
        return  # Avoid redundant stop signals

    try:
        # Format: XYYYZZZ (1 char for controller, 3 digits for amplitude, 3 digits for duration)
        formatted_data = f"{controller_id}{duty_cycle:03}{int(duration_ms):03}"
        sock_send.sendto(formatted_data.encode(), (broadcast_ip, pc_to_esp_port))  # Broadcast to ESP32
        print(f"[HAPTIC] Sent to {controller_id} controller: {formatted_data}")
    except Exception as e:
        print(f"[HAPTIC ERROR] {controller_id} controller: {e}")

# WebSocket event handling functions
def on_message(ws, message):
    try:
        data = json.loads(message)
        event_data = data.get("event_type", {}).get("data", {})
        path = event_data.get("path", "")
        duration = event_data.get("duration", {})
        amplitude = event_data.get("amplitude")

        controller_id = None
        if "/user/hand/right" in path:
            controller_id = "R"
        elif "/user/hand/left" in path:
            controller_id = "L"

        if controller_id and duration and amplitude is not None:
            duration_ms = (duration.get("secs", 0) * 1000) + (duration.get("nanos", 0) / 1e6)
            duty_cycle = int(amplitude * 255)

            # Limit values to prevent formatting issues
            duty_cycle = max(0, min(255, duty_cycle))
            duration_ms = max(0, min(999, duration_ms))  # Max duration 999ms to fit 3 digits

            send_to_esp32(controller_id, duty_cycle, duration_ms)
    except Exception as e:
        print(f"[ERROR] Processing WebSocket message: {e}")

def start_websocket():
    ws = websocket.WebSocketApp("ws://localhost:8082/api/events", on_message=on_message)
    ws.run_forever()

# Start ALVR sender thread
alvr_thread = threading.Thread(target=send_to_alvr, daemon=True)
alvr_thread.start()

# Start WebSocket listener in a separate thread
ws_thread = threading.Thread(target=start_websocket, daemon=True)
ws_thread.start()

# Start sending "START" until a controller response is received
start_signal_thread = threading.Thread(target=send_startup_signal, daemon=True)
start_signal_thread.start()

# Continuously listen for data from ESP32
try:
    while True:
        try:
            data, addr = sock_receive.recvfrom(1024)
            process_packet(data.decode())
        except Exception as e:
            print(f"[ERROR] Invalid Packet: {e}")
except KeyboardInterrupt:
    print("[EXIT] Putting NeoGrip to sleep...")
    for i in range(5):
        sock_send.sendto(b"STOP", (broadcast_ip, pc_to_esp_port))  # Send stop message on exit   
        print(".", end="", flush=True)  
        time.sleep(1)

finally:
    sock_receive.close()
    sock_send.close()
