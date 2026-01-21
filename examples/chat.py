import threading
import time
import sys
import json
import hashlib
import argparse
import kadepy._core as dht

# Constants
MSG_CHAT = 20
CHAT_TOPIC = hashlib.sha256(b"kadepy-chat-v1").digest()

known_peers = set() # (ip, port)
username = "User"
my_port = 0

def on_packet(sender_id, type, ip, port, payload):
    global known_peers
    # Add sender to known peers (if not self)
    if port != my_port:
        if (ip, port) not in known_peers:
            known_peers.add((ip, port))
            # print(f"\n[System] New peer: {ip}:{port}")

    if type == MSG_CHAT:
        try:
            # Payload is bytes, decode to string then JSON
            # Note: Payload might be None if empty
            if payload:
                # In Python C extension, payload is bytes
                data = json.loads(payload.decode('utf-8'))
                user = data.get('user', 'Unknown')
                msg = data.get('msg', '')
                print(f"\r[{user}] {msg}\n> ", end="")
        except Exception as e:
            # print(f"Error parsing chat: {e}")
            pass
            
    elif type == 6: # MSG_PEERS
        if payload:
            for peer in payload:
                p_ip = peer['ip']
                p_port = peer['port']
                if p_port != my_port:
                    known_peers.add((p_ip, p_port))

def discovery_loop(bootstrap_ip, bootstrap_port):
    while True:
        # 1. Announce/Get from Bootstrap
        if bootstrap_port and bootstrap_port != my_port:
             # Announce myself to bootstrap
             try:
                dht.dht_announce_peer(bootstrap_ip, bootstrap_port, CHAT_TOPIC, my_port)
                dht.dht_get_peers(bootstrap_ip, bootstrap_port, CHAT_TOPIC)
             except: pass
        
        # 2. Gossip with known peers
        # We don't want to flood, so pick random subset? 
        # For simple chat, just query all (small scale)
        for ip, port in list(known_peers):
             try:
                dht.dht_get_peers(ip, port, CHAT_TOPIC)
             except: pass
             
        # Cleanup old peers from C storage
        dht.storage_cleanup(60000) # Remove peers not seen for 60s
        
        time.sleep(10)

def main():
    global username, my_port
    parser = argparse.ArgumentParser()
    parser.add_argument("--user", default="Alice")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--bootstrap", type=int, default=0, help="Port of a known peer")
    parser.add_argument("--key", default="secret_chat_key")
    args = parser.parse_args()
    
    username = args.user
    
    # Setup Key
    key_bytes = args.key.encode('utf-8')
    if len(key_bytes) < 32:
        key_bytes += b'\0' * (32 - len(key_bytes))
    dht.set_network_key(key_bytes[:32])
    
    dht.set_callback(on_packet)
    my_port = dht.create_swarm(args.port)
    print(f"Chat started on port {my_port}. User: {username}")
    print(f"Encryption Key: {args.key}")
    
    if args.bootstrap:
        print(f"Bootstrapping from port {args.bootstrap}...")
        known_peers.add(("127.0.0.1", args.bootstrap))
    
    # Start Discovery Thread
    t = threading.Thread(target=discovery_loop, args=("127.0.0.1", args.bootstrap), daemon=True)
    t.start()
    
    print("Type message and press Enter. (Type /peers to list peers, /quit to exit)")
    print("> ", end="", flush=True)
    
    while True:
        try:
            msg = input()
            if msg == "/quit":
                break
            if msg == "/peers":
                print(f"Connected Peers: {len(known_peers)}")
                for p in known_peers: print(p)
                print("> ", end="", flush=True)
                continue
            
            if not msg:
                print("> ", end="", flush=True)
                continue
                
            payload = json.dumps({"user": username, "msg": msg}).encode('utf-8')
            
            # Broadcast to all known peers
            count = 0
            for ip, port in list(known_peers):
                dht.send_packet(ip, port, MSG_CHAT, payload)
                count += 1
            
            # Also send to bootstrap if not in known_peers yet
            if args.bootstrap and ("127.0.0.1", args.bootstrap) not in known_peers:
                 dht.send_packet("127.0.0.1", args.bootstrap, MSG_CHAT, payload)
            
            # Print own message
            # print(f"\r[You] {msg}\n> ", end="")
            print("> ", end="", flush=True)
            
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Error: {e}")

if __name__ == "__main__":
    main()
