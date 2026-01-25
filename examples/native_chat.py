import sys
import os
import time
import argparse
import threading

# Add parent directory to sys.path to prefer local source over installed package
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from kadepy.native_hyperswarm import NativeHyperswarm

# Topic for the chat (32-byte hex)
CHAT_TOPIC = "a" * 64 

def receive_loop(swarm):
    print("[System] Listening for messages...")
    while True:
        try:
            swarm.poll()
            time.sleep(0.01)
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"[Error] {e}")

def main():
    parser = argparse.ArgumentParser(description="Native Hyperswarm Chat")
    parser.add_argument("--mode", choices=["client", "bootstrap"], required=True, help="Mode: client or bootstrap")
    parser.add_argument("--port", type=int, default=0, help="Local port to bind (default: random)")
    parser.add_argument("--bootstrap-ip", default="127.0.0.1", help="Bootstrap Node IP (for client)")
    parser.add_argument("--bootstrap-port", type=int, default=10001, help="Bootstrap Node Port (for client)")
    
    args = parser.parse_args()
    
    swarm = NativeHyperswarm()
    
    if args.mode == "bootstrap":
        print(f"[System] Starting Private Bootstrap Node on port {args.bootstrap_port}...")
        # Port fixed, Isolated mode (empty bootstrap list)
        swarm.init_bootstrap_node(port=args.bootstrap_port, isolated=True)
        print(f"[System] Bootstrap Node Running on {swarm.get_port()}")
        
        # Bootstrap just sits there responding to DHT queries
        while True:
            swarm.poll()
            time.sleep(0.01)
            
    elif args.mode == "client":
        print(f"[System] Starting Chat Client on port {args.port}...")
        
        # 1. Add the Private Bootstrap Node to our routing table
        print(f"[System] Adding Bootstrap Node {args.bootstrap_ip}:{args.bootstrap_port}")
        swarm.add_peer(args.bootstrap_ip, args.bootstrap_port)
        
        # 2. Join the Chat Topic
        print(f"[System] Joining topic: {CHAT_TOPIC}")
        swarm.join(CHAT_TOPIC)
        
        # 3. Start Receive Thread
        t = threading.Thread(target=receive_loop, args=(swarm,), daemon=True)
        t.start()
        
        print("[System] Type a message and press Enter to send.")
        print("[System] (Wait for 'Handshake Established' log before sending)")
        
        while True:
            try:
                msg = input()
                if not msg: continue
                if msg == "/quit": break
                
                # Broadcast to all connected peers
                swarm.broadcast(msg)
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"[Error] {e}")

if __name__ == "__main__":
    main()
