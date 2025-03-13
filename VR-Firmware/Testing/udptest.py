import socket
import time

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

# Continuously send "Hello ESP" and listen for responses from ESP32
try:
    while True:
        # Send data to ESP32
        message_to_esp = b"Hello ESP"
        sock_send.sendto(message_to_esp, (broadcast_ip, pc_to_esp_port))  # Broadcast to ESP32
        
        # Receive data from ESP32
        sock_receive.settimeout(0.00001)  # Wait for 1 second for response
        try:
            data, addr = sock_receive.recvfrom(1024)
            print(f"Received from ESP32: {data.decode()}")
        except socket.timeout:
            print("No response from ESP32.")
        
except KeyboardInterrupt:
    print("Exiting...")
finally:
    sock_send.close()
    sock_receive.close()
