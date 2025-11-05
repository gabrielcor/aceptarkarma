import socket

# Define the target IP address and port
UDP_IP = "192.168.70.172"  # Replace with the server's IP address
UDP_PORT = 8080      # Replace with the server's listening port

# Define the message to send
MESSAGE = "Hello, UDP Server!"

# Create a UDP socket
# AF_INET specifies the address family (IPv4)
# SOCK_DGRAM specifies that it's a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    # Encode the message to bytes before sending
    encoded_message = MESSAGE.encode('utf-8')

    # Send the message to the specified IP and port
    sock.sendto(encoded_message, (UDP_IP, UDP_PORT))
    print(f"Sent UDP packet to {UDP_IP}:{UDP_PORT}: {MESSAGE}")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    # Close the socket
    sock.close()