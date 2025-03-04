import socket

# Define ports
pc_to_esp_port = 8888  # Port to send data from PC to ESP32
esp_to_pc_port = 9999  # Port to receive data from ESP32

# Broadcast IP address
broadcast_ip = '255.255.255.255'

# Create UDP socket for sending and receiving
sock_send = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_send.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

sock_receive = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_receive.bind(('', esp_to_pc_port))  # Listen on ESP32's response port

try:
    while True:
        # Send data to ESP32
        message_to_esp = b"Hello ESP"
        sock_send.sendto(message_to_esp, (broadcast_ip, pc_to_esp_port))

        # Receive data from ESP32
        sock_receive.settimeout(0.01)  # Small timeout to prevent blocking
        try:
            data, addr = sock_receive.recvfrom(1024)

            # Decode safely, ignoring errors
            button_state = data.decode(errors='ignore').strip()

            # Ensure only valid 2-character responses are processed
            if len(button_state) == 2 and button_state.isdigit():
                print(f"Button states from ESP32: {button_state}")
            else:
                print(f"Received invalid data: {data.hex()}")  # Print raw data for debugging

        except socket.timeout:
            pass  # No data received, continue loop

except KeyboardInterrupt:
    print("Exiting...")
finally:
    sock_send.close()
    sock_receive.close()
