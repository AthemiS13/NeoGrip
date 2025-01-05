import socket
import time
import requests

# Define the server and port for ALVR
alvr_url = "http://192.168.0.28:8082/api/set-buttons"  # ALVR Server Address
buffer_size = 1024  # Buffer size for receiving UDP packets
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind(('0.0.0.0', 8888))  # Listen on all network interfaces

print("Proxy server listening on port 8888...")

# Function to send button states to ALVR server
def send_to_alvr(trigger_value, grab_value):
    data = [
        {
            "path": "/user/hand/right/input/trigger/value",
            "value": {"Scalar": trigger_value}
        },
        {
            "path": "/user/hand/right/input/squeeze/value",
            "value": {"Scalar": grab_value}
        }
    ]
    try:
        response = requests.post(alvr_url, json=data)
        if response.status_code == 200:
            print(f"Data sent to ALVR: Trigger={trigger_value}, Grab={grab_value}")
        else:
            print(f"Failed to send data. Status code: {response.status_code}, Response: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"An error occurred: {e}")

# Main loop to process incoming UDP packets
while True:
    data, addr = udp_socket.recvfrom(buffer_size)
    print(f"Received data: {data.decode()} from {addr}")

    # Parse the packet (e.g., "01", "00", "10", "11")
    if data.decode() == "00":
        send_to_alvr(0.0, 0.0)  # Trigger off, Grab off
    elif data.decode() == "01":
        send_to_alvr(0.8, 0.0)  # Trigger on, Grab off
    elif data.decode() == "10":
        send_to_alvr(0.0, 1.0)  # Trigger off, Grab on
    elif data.decode() == "11":
        send_to_alvr(0.8, 1.0)  # Trigger on, Grab on
    else:
        print(f"Unknown packet received: {data.decode()}")


