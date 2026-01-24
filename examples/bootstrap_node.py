import time
import sys
import os

# Ensure we can import kadepy from parent directory
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from kadepy.native_hyperswarm import NativeHyperswarm

def main():
    print("Initializing Bootstrap Node...")
    swarm = NativeHyperswarm()
    
    # Initialize as Bootstrap Node on port 10001
    try:
        swarm.init_bootstrap_node(port=10001, isolated=True)
        print("Bootstrap Node Initialized Successfully!")
    except Exception as e:
        print(f"Failed to initialize bootstrap node: {e}")
        return

    # Verify port
    port = swarm.get_port()
    print(f"Node is listening on port: {port}")
    
    if port == 10001:
        print("SUCCESS: Port matches request (10001)")
    else:
        print(f"FAILURE: Port mismatch ({port} != 10001)")

    print("Running for 5 seconds...")
    for _ in range(5):
        swarm.poll()
        time.sleep(1)
        
    print("Exiting.")

if __name__ == "__main__":
    main()
