import socket
import threading
import time
import struct
import sys
import os

# Setup path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import kadepy._core as core

TEST_PORT = 46002
CLIENT_PORT = 57943

received_found_nodes = threading.Event()

def client_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('127.0.0.1', CLIENT_PORT))
    sock.settimeout(2.0)
    
    try:
        # 1. Enviar FIND_NODE
        print("[Client] Sending FIND_NODE...")
        target_id = b'\xBB' * 32
        
        # Type 2 (FIND_NODE) + Sender ID (fake) + Target ID
        find_pkt = b'\x02' + (b'\xCC' * 32) + target_id
        
        sock.sendto(find_pkt, ('127.0.0.1', TEST_PORT))
        
        # 2. Esperar FOUND_NODES
        data, addr = sock.recvfrom(2048)
        print(f"[Client] Received packet len={len(data)}")
        
        if len(data) > 33 and data[0] == 3: # MSG_FOUND_NODES
            count = data[33]
            print(f"[Client] SUCCESS: Received FOUND_NODES with {count} nodes!")
            
            if count >= 1:
                 # Parse first node to verify Endianness
                 # Header (33 header + 1 count) = 34 bytes
                 # Node = 32 id + 4 ip + 2 port = 38 bytes
                 offset = 34
                 node_id = data[offset:offset+32]
                 node_ip_raw = data[offset+32:offset+36]
                 node_port_raw = data[offset+36:offset+38]
                 
                 # Check IP (should be 127.0.0.1 -> 7F 00 00 01)
                 ip_str = socket.inet_ntoa(node_ip_raw)
                 port_val = struct.unpack('!H', node_port_raw)[0]
                 
                 print(f"[Client] Node 1: IP={ip_str} Port={port_val}")
                 
                 if ip_str == '127.0.0.1' and port_val == CLIENT_PORT:
                      print("[Client] Endianness check PASSED")
                      received_found_nodes.set()
                 else:
                      print(f"[Client] Endianness check FAILED. Expected 127.0.0.1:{CLIENT_PORT}")
        else:
            print(f"[Client] FAILED: Unexpected packet type {data[0]}")

    except socket.timeout:
        print("[Client] Timeout waiting for FOUND_NODES")
    except Exception as e:
        print(f"[Client] Error: {e}")
    finally:
        sock.close()

def on_packet(sender_id, type, ip, port, payload):
    pass # Ignorar callback para este teste

def test_find_node():
    print("[Python] Starting Swarm...")
    core.set_callback(on_packet)
    core.create_swarm(TEST_PORT)
    
    t = threading.Thread(target=client_listener)
    t.start()
    t.join()
    
    if received_found_nodes.is_set():
        print("TEST PASSED")
    else:
        print("TEST FAILED")
        exit(1)

if __name__ == "__main__":
    test_find_node()
