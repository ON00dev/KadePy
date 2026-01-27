import time
import sys
import os

# Add repo root to sys.path to load local kadepy
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../../')))

print(f"[Python Spy] sys.path: {sys.path}")
from kadepy.native_hyperswarm import NativeHyperswarm
import kadepy._hyperswarm
print(f"[Python Spy] Loaded _hyperswarm from: {kadepy._hyperswarm.__file__}")

def spy_test():
    print("[Python Spy] Starting...")
    node = NativeHyperswarm()
    node.init_bootstrap_node(0, 0)
    
    sniffer_port = 55575
    print(f"[Python Spy] Sending to 127.0.0.1:{sniffer_port}")
    
    # Send a simple packet
    # This should trigger udx_send with remote_id=0 (since peer not established)
    node.send_debug('127.0.0.1', sniffer_port, "Hello from KadePy")
    
    time.sleep(1)
    print("[Python Spy] Done")

if __name__ == "__main__":
    spy_test()
