import socket

# Configuration
target_ip = '224.0.0.1'  # Replace with your ESP32's IP address
target_port = 8888  # Port where the ESP32 listens for the "START" message

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Send the "START" message to the specified IP and port
message = b"START"
for i in range (5):
    # Send the message
    sock.sendto(message, (target_ip, target_port))

    print(f"Sent 'START' to {target_ip}:{target_port}")

# Close the socket
sock.close()
