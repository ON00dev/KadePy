
import sys
import os
import time
import subprocess
import kadepy

def run_server(port):
    print(f"[Server] Starting on port {port}")
    s = kadepy.Swarm(port=port)
    
    def on_packet(sender_id, msg_type, ip, port, payload):
        print(f"[Server] Received packet type {msg_type} from {ip}:{port}")
        # If it's a ping (msg_type usually 0 or 1, check protocol), we might auto-reply or just log
        
    s.set_callback(on_packet)
    
    # Keep alive
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[Server] Stopping")

def run_client(target_port):
    print(f"[Client] Starting client to ping port {target_port}")
    s = kadepy.Swarm(port=0)
    print(f"[Client] Bound to port {s.port}")
    
    received_response = False
    
    def on_packet(sender_id, msg_type, ip, port, payload):
        nonlocal received_response
        print(f"[Client] Received packet type {msg_type} from {ip}:{port}")
        received_response = True

    s.set_callback(on_packet)
    
    # Send Ping
    print(f"[Client] Pinging 127.0.0.1:{target_port}")
    s.ping("127.0.0.1", target_port)
    
    # Wait for response
    start = time.time()
    while time.time() - start < 5:
        if received_response:
            print("[Client] SUCCESS: Received response")
            sys.exit(0)
        time.sleep(0.1)
        
    print("[Client] FAILURE: Timed out waiting for response")
    sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "server":
        run_server(int(sys.argv[2]))
    else:
        # Start server process
        server_port = 8000
        server_process = subprocess.Popen([sys.executable, __file__, "server", str(server_port)])
        
        # Give server time to start
        time.sleep(1)
        
        try:
            # Run client logic in this process
            run_client(server_port)
        finally:
            server_process.terminate()
            server_process.wait()
