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
def send_to_alvr(controller, trigger_value, grab_value, button_primary, button_secondary, button_menu_or_sys, joyx_value, joyy_value, joyclk_value):
    user_hand_path = f"/user/hand/{controller}/input"
    primary_button_path = f"{user_hand_path}/{'x' if controller == 'left' else 'a'}/click"
    secondary_button_path = f"{user_hand_path}/{'y' if controller == 'left' else 'b'}/click"
    menu_or_sys_path = f"{user_hand_path}/{'menu' if controller == 'left' else 'system'}/click"
    
    data = [
        {"path": f"{user_hand_path}/trigger/value", "value": {"Scalar": trigger_value}},
        {"path": f"{user_hand_path}/squeeze/value", "value": {"Scalar": grab_value}},
        {"path": primary_button_path, "value": {"Binary": button_primary}},
        {"path": secondary_button_path, "value": {"Binary": button_secondary}},
        {"path": menu_or_sys_path, "value": {"Binary": button_menu_or_sys}},
        {"path": f"{user_hand_path}/thumbstick/x", "value": {"Scalar": joyx_value}},
        {"path": f"{user_hand_path}/thumbstick/y", "value": {"Scalar": joyy_value}},
        {"path": f"{user_hand_path}/thumbstick/click", "value": {"Binary": joyclk_value}}
    ]
    try:
        response = requests.post(alvr_url, json=data)
        if response.status_code == 200:
            print(f"Data sent to ALVR [{controller.upper()}]: Trigger={trigger_value}, Grab={grab_value}, Primary={button_primary}, Secondary={button_secondary}, Menu/System={button_menu_or_sys}, JOYX={joyx_value}, JOYY={joyy_value}, JOYCLK={joyclk_value}")
        else:
            print(f"Failed to send data [{controller.upper()}]. Status code: {response.status_code}, Response: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"An error occurred: {e}")

# Function to decode the received packet
def decode_packet(controller, packet):
    # Example packet: 'ABSYSTRIGSQZJOYXJOYYJOYCLK' (14 characters)
    button_primary = int(packet[0])  # 'A' or 'X' depending on the controller
    button_secondary = int(packet[1])  # 'B' or 'Y' depending on the controller
    button_menu_or_sys = int(packet[2])  # 1 or 0
    button_TRIG = int(packet[3])  # 1 or 0
    button_SQZ = int(packet[4])  # 1 or 0
    joyX = int(packet[5:9])  # 4 digit number (0-4095)
    joyY = int(packet[9:13])  # 4 digit number (0-4095)
    button_JOYCLK = int(packet[13])  # 1 or 0

    # Convert binary values to scalar
    trigger_value = 0.8 if button_TRIG == 1 else 0.0
    grab_value = 1.0 if button_SQZ == 1 else 0.0
    # Convert binary values of buttons to True/False for ALVR
    button_primary = button_primary == 1
    button_secondary = button_secondary == 1
    button_menu_or_sys = button_menu_or_sys == 1
    # Joystick values are already in the range 0-4095, so we need to scale them to -1 to 1
    joyx_value = (joyX - 2047) / 2047.0  # Normalize to the range -1.0 to 1.0
    joyy_value = (joyY - 2047) / 2047.0  # Normalize to the range -1.0 to 1.0
    joyclk_value = button_JOYCLK == 1  # Joystick click (True or False)

    # Send the values to ALVR
    send_to_alvr(controller, trigger_value, grab_value, button_primary, button_secondary, button_menu_or_sys, joyx_value, joyy_value, joyclk_value)

# Main loop to process incoming UDP packets
while True:
    data, addr = udp_socket.recvfrom(buffer_size)
    print(f"Received data: {data.decode()} from {addr}")

    # Check for valid packet length and controller identifier
    packet = data.decode()
    if len(packet) == 15:  # 14 characters + 1 identifier
        controller_id = packet[0]  # 'L' or 'R'
        if controller_id in ['L', 'R']:
            controller = "left" if controller_id == 'L' else "right"
            decode_packet(controller, packet[1:])  # Pass the rest of the packet
        else:
            print(f"Invalid controller identifier: {controller_id}")
    else:
        print(f"Invalid packet length: {len(packet)}")
