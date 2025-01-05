import socket
import time
import requests

# Define the server and port for ALVR
alvr_url = "http://192.168.0.28:8082/api/set-buttons"  # ALVR Server Address
buffer_size = 1024  # Buffer size for receiving UDP packets
udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_socket.bind(('0.0.0.0', 8888))  # Listen on all network interfaces

print("Proxy server listening on port 8888...")

# Function to send button states and joystick values to ALVR server
def send_to_alvr(trigger_value, grab_value, button_a, button_b, button_sys, joyx_value, joyy_value, joyclk_value):
    data = [
        {
            "path": "/user/hand/right/input/trigger/value",
            "value": {"Scalar": trigger_value}
        },
        {
            "path": "/user/hand/right/input/squeeze/value",
            "value": {"Scalar": grab_value}
        },
        {
            "path": "/user/hand/right/input/a/click",
            "value": {"Binary": button_a}
        },
        {
            "path": "/user/hand/right/input/b/click",
            "value": {"Binary": button_b}
        },
        {
            "path": "/user/hand/right/input/system/click",
            "value": {"Binary": button_sys}
        },
        {
            "path": "/user/hand/right/input/thumbstick/x",
            "value": {"Scalar": joyx_value}
        },
        {
            "path": "/user/hand/right/input/thumbstick/y",
            "value": {"Scalar": joyy_value}
        },
        {
            "path": "/user/hand/right/input/thumbstick/click",
            "value": {"Binary": joyclk_value}
        }
    ]
    try:
        response = requests.post(alvr_url, json=data)
        if response.status_code == 200:
            print(f"Data sent to ALVR: Trigger={trigger_value}, Grab={grab_value}, A={button_a}, B={button_b}, SYS={button_sys}, JOYX={joyx_value}, JOYY={joyy_value}, JOYCLK={joyclk_value}")
        else:
            print(f"Failed to send data. Status code: {response.status_code}, Response: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"An error occurred: {e}")

# Function to decode the received packet
def decode_packet(packet):
    # Example packet: 'ABSYSTRIGSQZJOYXJOYYJOYCLK' (14 characters)
    button_A = int(packet[0])  # 1 or 0
    button_B = int(packet[1])  # 1 or 0
    button_SYS = int(packet[2])  # 1 or 0
    button_TRIG = int(packet[3])  # 1 or 0
    button_SQZ = int(packet[4])  # 1 or 0
    joyX = int(packet[5:9])  # 4 digit number (0-4095)
    joyY = int(packet[9:13])  # 4 digit number (0-4095)
    button_JOYCLK = int(packet[13])  # 1 or 0

    # Convert binary values to scalar
    trigger_value = 0.8 if button_TRIG == 1 else 0.0
    grab_value = 1.0 if button_SQZ == 1 else 0.0
    # Convert binary values of buttons to True/False for ALVR
    button_a = True if button_A == 1 else False
    button_b = True if button_B == 1 else False
    button_sys = True if button_SYS == 1 else False
    # Joystick values are already in the range 0-4095, so we need to scale them to -1 to 1
    joyx_value = (joyX - 2047) / 2047.0  # Normalize to the range -1.0 to 1.0
    joyy_value = (joyY - 2047) / 2047.0  # Normalize to the range -1.0 to 1.0
    joyclk_value = True if button_JOYCLK == 1 else False  # Joystick click (True or False)

    # Send the values to ALVR
    send_to_alvr(trigger_value, grab_value, button_a, button_b, button_sys, joyx_value, joyy_value, joyclk_value)

# Main loop to process incoming UDP packets
while True:
    data, addr = udp_socket.recvfrom(buffer_size)
    print(f"Received data: {data.decode()} from {addr}")

    # Check if the received data is exactly 14 characters long
    packet = data.decode()
    if len(packet) == 14:
        # Decode and send values to ALVR
        decode_packet(packet)
    else:
        print(f"Invalid packet length: {len(packet)}")
