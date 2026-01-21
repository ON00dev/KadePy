import socket
import threading
import time
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import kadepy._core as core

TEST_PORT = 45000
CLIENT_PORT = 56000

received_pong = threading.Event()

def client_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', CLIENT_PORT))
    sock.settimeout(2.0)
    
    try:
        # Send PING
        print("[Client] Sending PING...")
        # Type 0 (PING) + Sender ID (random)
        ping_pkt = b'\x00' + (b'\xAA' * 32)
        sock.sendto(ping_pkt, ('127.0.0.1', TEST_PORT))
        
        # Expect PONG
        data, addr = sock.recvfrom(1024)
        print(f"[Client] Received {len(data)} bytes")
        
        if len(data) >= 33 and data[0] == 1: # MSG_PONG
            print("[Client] Received PONG!")
            received_pong.set()
            
    except Exception as e:
        print(f"[Client] Error: {e}")
    finally:
        sock.close()

def test_pong():
    print("[Python] Starting Swarm...")
    core.create_swarm(TEST_PORT)
    
    t = threading.Thread(target=client_listener)
    t.start()
    t.join()
    
    if received_pong.is_set():
        print("TEST PASSED")
    else:
        print("TEST FAILED")
        exit(1)

if __name__ == "__main__":
    test_pong()
