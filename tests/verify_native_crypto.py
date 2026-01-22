import sys
import os
import time

# Ensure we can import the extension from kadepy/
sys.path.insert(0, os.path.abspath("."))

try:
    from kadepy._hyperswarm import HyperswarmNode
    print("Module imported successfully.")
except ImportError as e:
    print(f"Failed to import _hyperswarm: {e}")
    sys.exit(1)

def run_test():
    print("Creating Node A...")
    node_a = HyperswarmNode()
    port_a = node_a.get_port()
    print(f"Node A bound to port {port_a}")

    print("Creating Node B...")
    node_b = HyperswarmNode()
    port_b = node_b.get_port()
    print(f"Node B bound to port {port_b}")
    
    # Add B as peer to A (manually, to skip DHT lookup for this test)
    # This should update routing table so when A joins, it finds B (if we implement that logic)
    # But currently add_peer just updates routing table.
    # hyperswarm_join sends FIND_NODE to closest peers.
    
    print(f"Adding B ({port_b}) to A's routing table...")
    node_a.add_peer("127.0.0.1", port_b)
    
    # Topic
    topic = "a" * 64 # 32 bytes hex
    
    print("Node A joining topic...")
    node_a.join(topic)
    
    # Loop to poll
    print("Polling for 5 seconds...")
    for i in range(50):
        node_a.poll()
        node_b.poll()
        time.sleep(0.1)
        
        # At step 20 (approx 2s), send a message from A to B
        if i == 20:
             print("\n[Test] Sending encrypted message from A to B...")
             node_a.send_debug("127.0.0.1", port_b, "Hello Encrypted World!")

    print("Test finished.")

if __name__ == "__main__":
    run_test()
