import sys
import time
import argparse
import kadepy._core as dht
import binascii

def on_packet(sender_id, type, ip, port, payload):
    # print(f"[Node] Packet {type} from {ip}:{port}")
    if type == 6: # MSG_PEERS
        print(f"FOUND_PEERS: {payload}")
    elif type == 3: # MSG_FOUND_NODES
        print(f"FOUND_NODES: {len(payload)}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--key", type=str, default="none")
    parser.add_argument("--target-port", type=int, default=0)
    parser.add_argument("--action", type=str, default="listen")
    parser.add_argument("--topic", type=str, default="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") # 32 bytes hex
    
    args = parser.parse_args()
    
    # Setup Callback
    dht.set_callback(on_packet)
    
    # Setup Key
    if args.key.lower() != "none":
        key_bytes = args.key.encode('utf-8')
        # Pad to 32 bytes
        if len(key_bytes) < 32:
            key_bytes = key_bytes + b'\0' * (32 - len(key_bytes))
        dht.set_network_key(key_bytes)
        print(f"Network Key Set: {key_bytes[:5]}...")
    else:
        dht.set_network_key(None)
        print("Network Key Disabled")

    # Create Swarm
    real_port = dht.create_swarm(args.port)
    print(f"Node listening on {real_port}")
    
    # Action
    topic_bytes = binascii.unhexlify(args.topic)
    
    if args.action == "listen":
        while True:
            time.sleep(1)
            
    elif args.action == "announce":
        if args.target_port == 0:
            print("Error: Target port required for announce")
            return
        print(f"Announcing topic to 127.0.0.1:{args.target_port}")
        # Announce self as peer (port=real_port)
        dht.dht_announce_peer("127.0.0.1", args.target_port, topic_bytes, real_port)
        time.sleep(2) # Give time to send
        
    elif args.action == "get":
        if args.target_port == 0:
            print("Error: Target port required for get")
            return
        print(f"Getting peers from 127.0.0.1:{args.target_port}")
        dht.dht_get_peers("127.0.0.1", args.target_port, topic_bytes)
        # Wait for response
        time.sleep(3)

if __name__ == "__main__":
    main()
