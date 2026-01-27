import sys
import os
import time
import unittest

# Ensure kadepy is in path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from kadepy.native_hyperswarm import NativeHyperswarm

class TestJSBinding(unittest.TestCase):
    def test_lifecycle(self):
        print("Initializing NativeHyperswarm (JS Daemon)...")
        node = NativeHyperswarm()
        
        # Check if internal port is set
        print(f"Internal TCP Port: {node.port}")
        self.assertTrue(node.port > 0)
        
        # Check if DHT port is retrievable
        # This might fail if DHT is not ready, but we try.
        dht_port = node.get_port()
        print(f"DHT Port: {dht_port}")
        # Note: dht_port might be 0 or null if lazy binding
        
        # Join a topic
        topic = "a" * 64
        print(f"Joining topic: {topic}")
        node.join(topic)
        
        # Wait a bit
        time.sleep(2)
        
        # Leave
        print("Leaving topic...")
        node.leave(topic)
        
        # Destroy
        print("Destroying node...")
        node.destroy()
        print("Done.")

if __name__ == '__main__':
    unittest.main()
