import subprocess
import time
import sys
import re

PYTHON_EXE = sys.executable
SCRIPT_PATH = "tests/node_script.py"

def start_node(key, action="listen", target_port=0, topic=None):
    cmd = [PYTHON_EXE, "-u", SCRIPT_PATH, "--key", key, "--action", action]
    if target_port:
        cmd.extend(["--target-port", str(target_port)])
    if topic:
        cmd.extend(["--topic", topic])
        
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return proc

def get_port(proc):
    start_time = time.time()
    while time.time() - start_time < 5:
        line = proc.stdout.readline()
        if not line:
            break
        print(f"[Node Output] {line.strip()}")
        match = re.search(r"Node listening on (\d+)", line)
        if match:
            return int(match.group(1))
    return None

def main():
    print("=== Starting Encryption Integration Test ===")
    
    # 1. Start Storage Node (Key=secret)
    print("\n1. Starting Storage Node (Key=secret)...")
    storage_proc = start_node("secret")
    storage_port = get_port(storage_proc)
    if not storage_port:
        print("Failed to start storage node")
        return
    print(f"Storage Node running on {storage_port}")
    
    topic = "aa" * 32
    
    # 2. Announce (Key=secret)
    print("\n2. Announcing Topic (Key=secret)...")
    announcer = start_node("secret", action="announce", target_port=storage_port, topic=topic)
    announcer.wait()
    print("Announce completed.")
    
    # 3. Retrieve (Key=secret) - Should Success
    print("\n3. Retrieving Topic (Key=secret)...")
    retriever = start_node("secret", action="get", target_port=storage_port, topic=topic)
    
    success = False
    start_time = time.time()
    while time.time() - start_time < 5:
        line = retriever.stdout.readline()
        if not line: break
        print(f"[Retriever] {line.strip()}")
        if "FOUND_PEERS" in line:
            success = True
            break
    
    retriever.terminate()
    if success:
        print("SUCCESS: Valid peers received.")
    else:
        print("FAILURE: No peers received.")
        
    # 4. Retrieve (Key=wrong) - Should Fail
    print("\n4. Retrieving Topic (Key=wrong)...")
    intruder = start_node("wrong", action="get", target_port=storage_port, topic=topic)
    
    intruder_success = False
    start_time = time.time()
    while time.time() - start_time < 5:
        line = intruder.stdout.readline()
        if not line: break
        print(f"[Intruder] {line.strip()}")
        if "FOUND_PEERS" in line:
            intruder_success = True
            break
            
    intruder.terminate()
    if intruder_success:
        print("FAILURE: Intruder received peers!")
    else:
        print("SUCCESS: Intruder received nothing (or garbage).")

    # Cleanup
    storage_proc.terminate()
    print("\nTest Complete.")

if __name__ == "__main__":
    main()
