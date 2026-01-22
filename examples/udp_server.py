import socket
import sys

def main():
    port = 8000
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', port))
    print(f"Listening on port {port}...")

    while True:
        try:
            data, addr = sock.recvfrom(1024)
            print(f"Received {len(data)} bytes from {addr}: {data}")
            
            # Echo back to punch hole
            # Send UDX packet type HOLEPUNCH (0x04) or just raw data
            # Native client expects ANY packet to confirm connection.
            
            # Construct a minimal UDX header + payload
            # Header is 13 bytes: type(1), conn_id(4), seq(4), ack(4)
            # We can just send raw bytes, as long as it's valid enough or if we don't care about header validation in `udx_recv`
            # Wait, `udx_recv` checks size >= 13.
            
            # Send UDX HOLEPUNCH (0x84)
            # Header is 13 bytes: type(1), conn_id(4), seq(4), ack(4)
            response_udx = b'\x84' + b'\x00'*12 + b'PONG'
            sock.sendto(response_udx, addr)
            print(f"Sent UDX HOLEPUNCH to {addr}")

            # Send DHT FIND_NODE (Type 2)
            # Header: Type(2) + PK(32) + TS(8) + Sig(64) = 105 bytes
            # Payload: Target ID (32 bytes)
            dht_header = b'\x02' + b'S'*32 + b'\x00'*8 + b'\x00'*64
            dht_payload = b'T'*32 # Target ID
            sock.sendto(dht_header + dht_payload, addr)
            print(f"Sent DHT FIND_NODE to {addr}")
            
        except KeyboardInterrupt:
            break

if __name__ == "__main__":
    main()
