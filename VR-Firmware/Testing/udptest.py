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

print("Listening for button states from ESP32...")

try:
    while True:
        # Send data to ESP32
        message_to_esp = b"Hello ESP"
        sock_send.sendto(message_to_esp, (broadcast_ip, pc_to_esp_port))  # Broadcast to ESP32
        
        # Receive button states from ESP32
        sock_receive.settimeout(0.00001)  # Non-blocking receive
        try:
            data, addr = sock_receive.recvfrom(1024)
            print(f"Button states from ESP32: {data.decode()}")  # Print "00", "01", etc.
        except socket.timeout:
            pass  # Ignore if no data received

        time.sleep(0)  # 50ms delay to prevent spamming
except KeyboardInterrupt:
    print("Exiting...")
finally:
    sock_send.close()
    sock_receive.close()
