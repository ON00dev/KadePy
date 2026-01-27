import time
import hashlib
import os
import sys

# Add repo root to sys.path to load local kadepy
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../../')))

from kadepy.swarm import Swarm

def test_interop():
    print("[Python] Starting KadePy Swarm...")
    swarm = Swarm()
    
    topic = hashlib.sha256(b'kadepy-native-test').digest()
    topic_hex = topic.hex()
    
    print(f"[Python] Joining topic: {topic_hex}")
    swarm.join(topic_hex)
    
    connected_peers = []
    
    def on_connection(socket, info):
        print(f"[Python] New connection! Info: {info}")
        connected_peers.append(socket)
        
        # Send greeting
        try:
            socket.sendall(b"Hello from KadePy!")
        except Exception as e:
            print(f"[Python] Send error: {e}")

        # Listen for data (simple loop for demo)
        def listen_socket(s):
            while True:
                try:
                    data = s.recv(1024)
                    if not data: break
                    print(f"[Python] Received: {data.decode()}")
                except Exception as e:
                    break
        
        import threading
        t = threading.Thread(target=listen_socket, args=(socket,), daemon=True)
        t.start()

    swarm.on('connection', on_connection)
    
    # Wait for connections
    print("[Python] Waiting for connections...")
    for i in range(30):
        if connected_peers:
            print("[Python] Peer connected!")
            break
        time.sleep(1)
        
    if not connected_peers:
        print("[Python] Timeout waiting for peers.")
    else:
        # Keep alive for a bit to exchange messages
        time.sleep(5)
        
    print("[Python] Closing swarm...")
    swarm.close()

if __name__ == "__main__":
    test_interop()
