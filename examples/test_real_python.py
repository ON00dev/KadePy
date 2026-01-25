import sys
import os
import time
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from kadepy.swarm import Swarm
import hashlib

def main():
    print("Python Peer starting...")
    swarm = Swarm()
    
    topic = hashlib.sha256(b"kadepy-interop-test").hexdigest()
    print(f"Joining topic: {topic}")
    
    swarm.join(topic)
    
    def on_data(data):
        print(f"Received from Node.js: {data}")
        if "Hello from Node.js" in str(data):
            print("SUCCESS: Received hello from Node.js")
            # Reply
            swarm.send("Hello from Python")
            print("Sent reply. Exiting in 3s...")
            time.sleep(3)
            os._exit(0)

    swarm.on("data", on_data)
    
    # Keep running
    try:
        # Give some time to connect
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
