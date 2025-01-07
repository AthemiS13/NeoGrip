import socket
import threading
import requests
import time
from queue import Queue

# ALVR Server configuration
alvr_url = "http://192.168.0.28:8082/api/set-buttons"
send_interval = 0.04  # Minimum interval (40ms) between updates to ALVR
buffer_size = 1024  # UDP buffer size

# Ports for controllers
controller_ports = {"left": 8888, "right": 8889}

# Shared state
state_queue = Queue()
last_sent_data = {"left": None, "right": None}


# Function to send button states and joystick values to ALVR server
def send_to_alvr(controller, data):
    try:
        response = requests.post(alvr_url, json=data)
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
            data, addr = udp_socket.recvfrom(buffer_size)
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

                # Only send data if it's different from the last sent state
                if data != last_sent_data[controller]:
                    send_to_alvr(controller, data)
                    last_sent_data[controller] = data
            time.sleep(send_interval)
        except KeyboardInterrupt:
            print("Stopping ALVR updater.")
            break

# Main function
def main():
    try:
        # Start listener threads for each controller
        listener_threads = []
        for controller, port in controller_ports.items():
            thread = threading.Thread(target=listen_for_controller, args=(controller, port))
            thread.daemon = True
            listener_threads.append(thread)
            thread.start()

        # Start the ALVR updater thread
        updater_thread = threading.Thread(target=update_alvr)
        updater_thread.daemon = True
        updater_thread.start()

        print("Proxy server running. Press Ctrl+C to stop.")
        while True:
            time.sleep(1)  # Keep the main thread alive
    except KeyboardInterrupt:
        print("Stopping proxy server...")
    finally:
        for thread in listener_threads:
            thread.join(timeout=1)

if __name__ == "__main__":
    main()
