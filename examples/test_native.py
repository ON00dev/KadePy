import time
import sys
import os

try:
    from kadepy.native_hyperswarm import NativeHyperswarm
except ImportError:
    sys.path.insert(0, os.getcwd())
    from kadepy.native_hyperswarm import NativeHyperswarm

def main():
    print("Initializing Native Hyperswarm Node...")
    node = NativeHyperswarm()
    
    port = node.get_port()
    print(f"Native Node bound to port: {port}")

    # Manually add a peer to the DHT routing table
    # This simulates finding a peer via bootstrap or DHT lookup
    peer_ip = "127.0.0.1"
    peer_port = 8000
    print(f"Adding peer {peer_ip}:{peer_port} to routing table...")
    node.add_peer(peer_ip, peer_port)

    topic = "9b1c770737603c40333273185573489816654261b40209677317208035223013"
    print(f"Joining topic: {topic}")
    node.join(topic)
    
    print("Polling for events (Ctrl+C to stop)...")
    try:
        for i in range(10):
            node.poll()
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nStopping...")

    print("Test completed successfully.")

if __name__ == "__main__":
    main()
