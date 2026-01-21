
import sys
import os
import time
import subprocess
import kadepy

TOPIC_HASH = b'\x01' * 32

def run_server(port):
    print(f"[Server] Starting on port {port}")
    s = kadepy.Swarm(port=port)
    
    def on_packet(sender_id, msg_type, ip, port, payload):
        # print(f"[Server] Received packet type {msg_type} from {ip}:{port}")
        pass
        
    s.set_callback(on_packet)
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def run_announcer(bootstrap_port):
    print(f"[Announcer] Starting...")
    s = kadepy.Swarm(port=0)
    print(f"[Announcer] Port: {s.port}")
    
    s.bootstrap("127.0.0.1", bootstrap_port)
    time.sleep(1) # Wait for ping/pong
    
    print(f"[Announcer] Announcing topic...")
    s.announce(TOPIC_HASH)
    
    print(f"[Announcer] Announced. Waiting for queries...")
    try:
        time.sleep(10)
    except KeyboardInterrupt:
        pass

def run_getter(bootstrap_port):
    print(f"[Getter] Starting...")
    s = kadepy.Swarm(port=0)
    print(f"[Getter] Port: {s.port}")
    
    s.bootstrap("127.0.0.1", bootstrap_port)
    time.sleep(1)
    
    found_peers = []
    done_event = False
    
    def on_packet(sender_id, msg_type, ip, port, payload):
        nonlocal done_event
        if msg_type == kadepy.swarm.MSG_PEERS:
            print(f"[Getter] Received PEERS: {payload}")
            found_peers.extend(payload)
            done_event = True
            
    s.set_callback(on_packet)
    
    print(f"[Getter] Getting peers...")
    s.get_peers(TOPIC_HASH)
    
    start = time.time()
    while time.time() - start < 5:
        if done_event:
            break
        time.sleep(0.1)
        
    if found_peers:
        print("[Getter] SUCCESS: Found peers!")
        sys.exit(0)
    else:
        print("[Getter] FAILURE: No peers found.")
        sys.exit(1)

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "main"
    
    if mode == "server":
        run_server(8000)
    elif mode == "announcer":
        run_announcer(8000)
    elif mode == "getter":
        run_getter(8000)
    else:
        # Orchestrator
        server = subprocess.Popen([sys.executable, __file__, "server"])
        time.sleep(1)
        
        announcer = subprocess.Popen([sys.executable, __file__, "announcer"])
        time.sleep(2)
        
        getter = subprocess.Popen([sys.executable, __file__, "getter"])
        
        code = getter.wait()
        
        announcer.terminate()
        server.terminate()
        
        if code == 0:
            print("TEST PASSED")
        else:
            print("TEST FAILED")
        sys.exit(code)
