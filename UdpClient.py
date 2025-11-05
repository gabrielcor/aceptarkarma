import socket
import time

# Define the target IP address and port
UDP_IP = "192.168.20.99"  # Replace with the server's IP address
UDP_PORT = 8080            # Replace with the server's listening port

# Define the message to send
MESSAGE = "Hello, UDP Server!"

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    encoded_message = MESSAGE.encode('utf-8')
    print(f"Starting to send UDP messages every second to {UDP_IP}:{UDP_PORT}...")

    while True:
        sock.sendto(encoded_message, (UDP_IP, UDP_PORT))
        print(f"Sent UDP packet to {UDP_IP}:{UDP_PORT}: {MESSAGE}")
        time.sleep(1)  # Wait for 1 second

except KeyboardInterrupt:
    print("Stopped by user.")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    sock.close()
