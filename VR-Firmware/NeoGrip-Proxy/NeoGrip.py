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
# Headers for ALVR requests because of recent changes in ALVR API
headers = {
    "X-ALVR": "true",
    "Content-Type": "application/json"
    }



# Create UDP sockets
sock_receive = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_receive.settimeout(1.0)  # Timeout to allow Ctrl+C interruption on Windows
sock_receive.bind(('', esp_to_pc_port))  # Listen for packets from ESP32

sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_send.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

# Queue to process controller states
state_queue = queue.Queue()

# Set to keep track of connected controllers
connected_controllers = set()
controller_ips = set()
last_connection_time = time.time()

# Function to send initialization/shutdown messages
def send_startup_signal():
    print("[INIT] Press system button on controller(s) to connect")
    global connected_controllers, last_connection_time
    
    while True:
        if len(connected_controllers) == 2:
            print("\n[INIT] Both controllers connected. Stopping START broadcast.")
            break
            
        # If one controller has been connected for more than 15 seconds and the other hasn't, 
        # assume single controller mode and stop broadcasting.
        if len(connected_controllers) == 1 and (time.time() - last_connection_time > 15):
            print("\n[INIT] 15s timeout reached. Continuing with single controller. Stopping START broadcast.")
            break

        try:
            sock_send.sendto(b"START", (broadcast_ip, pc_to_esp_port))
            if len(connected_controllers) == 0:
                print(".", end="", flush=True)
            time.sleep(0.5)
        except Exception as e:
            print(f"[ERROR] Failed to send START: {e}")
            time.sleep(0.5)


# Packet processing function
def process_packet(packet, addr=None):
    global connected_controllers, last_connection_time, controller_ips
    try:
        print(f"[ESP] Raw data: {packet}")

        if len(packet) != 15:
            print(f"[ERROR] Invalid packet length: {len(packet)}")
            return

        controller_id = packet[0]  # 'L' or 'R'
        
        if controller_id not in connected_controllers:
            connected_controllers.add(controller_id)
            if addr:
                controller_ips.add(addr[0])
            last_connection_time = time.time()
            if len(connected_controllers) == 1:
                print(f"\n[ESP] Controller {controller_id} connected. Waiting for second controller...")

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
            {"path": f"{hand_path}/input/trigger/value", "value": {"Scalar": 1.0 if button_TRIG else 0.0}},
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
            response = requests.post(alvr_url, headers=headers, json=data, timeout=0.1)
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
    while True:
        try:
            ws = websocket.WebSocketApp("ws://localhost:8082/api/events", on_message=on_message)
            ws.run_forever()
        except Exception as e:
            print(f"[WS ERROR] Connection failed: {e}")
        time.sleep(2)

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
            process_packet(data.decode(), addr)
        except socket.timeout:
            pass # Timeout triggered to allow Ctrl+C check
        except Exception as e:
            print(f"[ERROR] Invalid Packet: {e}")
except KeyboardInterrupt:
    print("\n[EXIT] Putting NeoGrip to sleep...")
    try:
        for i in range(10):
            sock_send.sendto(b"STOP", (broadcast_ip, pc_to_esp_port))  # Send broadcast stop
            for ip in controller_ips:
                sock_send.sendto(b"STOP", (ip, pc_to_esp_port))  # Send unicast stop directly
            print(".", end="", flush=True)  
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass # Allow user to force exit by pressing Ctrl+C again
finally:
    sock_receive.close()
    sock_send.close()