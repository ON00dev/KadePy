import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import kadepy

print("Successfully imported kadepy")
s = kadepy.Swarm()
print("Swarm instance created")
