import socket
import threading
import time
import struct
import sys
import os

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import kadepy._core as core

received_events = []
event_lock = threading.Lock()

def on_packet(sender_id, msg_type, ip, port, payload):
    with event_lock:
        print(f"[Callback] Type={msg_type} Payload={payload}")
        received_events.append({'type': msg_type, 'payload': payload})

def test_payload():
    print("[Test] Starting Swarm...")
    core.set_callback(on_packet)
    port = core.create_swarm(0)
    print(f"[Test] Swarm on port {port}")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        # 1. Simulate MSG_PEERS (Type 6)
        # Header: Type(1) + Sender(32)
        # Payload: Count(1) + N * (IP(4) + Port(2))
        
        print("[Test] Sending MSG_PEERS...")
        sender_id = b'\x11' * 32
        header = b'\x06' + sender_id
        
        # Peers: 10.0.0.1:8001, 192.168.1.5:8002
        peers = []
        peers.append(socket.inet_aton('10.0.0.1') + struct.pack('!H', 8001))
        peers.append(socket.inet_aton('192.168.1.5') + struct.pack('!H', 8002))
        
        payload_peers = b'\x02' + b''.join(peers)
        sock.sendto(header + payload_peers, ('127.0.0.1', port))
        
        time.sleep(0.5)
        
        # 2. Simulate MSG_FOUND_NODES (Type 3)
        # Header: Type(1) + Sender(32)
        # Payload: Count(1) + N * (ID(32) + IP(4) + Port(2))
        
        print("[Test] Sending MSG_FOUND_NODES...")
        header_nodes = b'\x03' + sender_id
        
        nodes = []
        node_id = b'\xEE' * 32
        nodes.append(node_id + socket.inet_aton('8.8.8.8') + struct.pack('!H', 53))
        
        payload_nodes = b'\x01' + b''.join(nodes)
        sock.sendto(header_nodes + payload_nodes, ('127.0.0.1', port))
        
        time.sleep(0.5)
        
        print("[Test] Verifying results...")
        with event_lock:
            # Check MSG_PEERS
            peers_evt = next((e for e in received_events if e['type'] == 6), None)
            if not peers_evt:
                print("FAILED: No MSG_PEERS received")
                exit(1)
            
            p_payload = peers_evt['payload']
            print(f"[Test] Peers Payload: {p_payload}")
            if len(p_payload) != 2:
                print("FAILED: Expected 2 peers")
                exit(1)
                
            peer1 = p_payload[0]
            print(f"[Test] Peer 1: {peer1}")
            if peer1['ip'] == '10.0.0.1' and peer1['port'] == 8001:
                print("[Test] Peer 1 matches!")
            else:
                print("FAILED: Peer 1 mismatch")
                exit(1)

            # Check MSG_FOUND_NODES
            nodes_evt = next((e for e in received_events if e['type'] == 3), None)
            if not nodes_evt:
                print("FAILED: No MSG_FOUND_NODES received")
                exit(1)
            
            n_payload = nodes_evt['payload']
            print(f"[Test] Nodes Payload: {n_payload}")
            if len(n_payload) != 1:
                print("FAILED: Expected 1 node")
                exit(1)
                
            node1 = n_payload[0]
            if node1['id'] == node_id and node1['ip'] == '8.8.8.8' and node1['port'] == 53:
                print("[Test] Node 1 matches!")
            else:
                print("FAILED: Node 1 mismatch")
                exit(1)

        print("TEST PASSED")

    finally:
        sock.close()

if __name__ == "__main__":
    test_payload()
