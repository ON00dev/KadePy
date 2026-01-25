# KadePy

**High-Performance Kademlia DHT Implementation for Python**

KadePy is a robust and efficient Distributed Hash Table (DHT) library implementing the Kademlia protocol. It combines a high-performance C extension core for network handling and routing with a user-friendly Python wrapper.

## Features

- **High Performance**: Core logic (UDP reactor, routing table, storage, protocol) implemented in C for minimal overhead and maximum throughput.
- **Cross-Platform**: Fully compatible with Windows, Linux, and macOS.
- **Hyperswarm Integration (v0.2.3)**:
  - **Transparent Bridge**: Automatically launches a managed Node.js bridge to leverage the battle-tested JS Hyperswarm implementation.
  - **Full Compatibility**: Connect seamlessly with existing Hyperswarm networks (Node.js, Holepunch).
  - **Auto-Setup**: Automatically handles dependency installation (`npm install`) and process management.
- **Native Extensions**:
  - **Noise Handshake (XX Pattern)**: Secure, authenticated connections using `libsodium` (Ed25519/Curve25519).
  - **UDX Transport**: Reliable, encrypted UDP transport with congestion control and packet ordering.
  - **Holepunching**: Built-in NAT traversal.
  - **Private Bootstrap Mode**: Run isolated nodes on fixed ports to serve as private network beacons.
- **Secure**:
  - Uses **XSalsa20-Poly1305** for transport encryption.
  - Implements secure random number generation.
- **Concurrency**: Thread-safe architecture with a dedicated network reactor thread, releasing the GIL whenever possible.
- **Easy to Use**: Simple Python API for creating nodes, storing values, and finding peers.

## Installation

### From Source

```bash
git clone https://github.com/ON00dev/KadePy.git
cd KadePy
pip install .
```

### Requirements

- Python 3.10+
- C Compiler (GCC/Clang on Linux/macOS, MSVC on Windows)
- Libsodium (automatically bundled on Windows if built via setup.py)

## Quick Start

### Basic Node (Hyperswarm)

```python
from kadepy import Swarm
import hashlib
import time

# Create a node (automatically starts the Node.js bridge)
node = Swarm()
print("Node started!")

# Generate a topic key (32-byte bytes)
topic = hashlib.sha256(b"my-app-topic").digest()
print(f"Joining topic: {topic.hex()}")

# Join the swarm for this topic (announce=True, lookup=True by default)
node.join(topic)

# Define callback for incoming data
def on_data(data):
    print(f"Received message: {data}")
    # Reply to the sender (broadcasts to topic in this simple example)
    node.send(f"Echo: {data}")

# Register the callback
node.on("data", on_data)

# Send a message to the swarm
node.send("Hello World from KadePy!")

# Keep the process alive
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Stopping...")
```

## Architecture

KadePy follows a hybrid architecture:

1.  **C Extension (`_core`)**:
    *   **UDP Reactor**: Handles non-blocking socket I/O using `select` (or platform specific polling).
    *   **Protocol**: Implements Kademlia message serialization/deserialization.
    *   **Routing**: Manages K-Buckets and node lookups.
    *   **Storage**: In-memory key-value storage with expiration.
    *   **Crypto**: ChaCha20 encryption and secure RNG.
    8.  **Hyperswarm (Experimental)**: Native C implementation of Noise/UDX (work in progress).

2.  **Node.js Bridge (`kadepy.js`)**:
    *   **Bridge Process**: A lightweight Node.js process managed by Python.
    *   **Hyperswarm**: Uses the official `hyperswarm` library for DHT discovery and holepunching.
    *   **TCP IPC**: Communicates with Python via a local TCP connection using JSON messages.

3.  **Python Wrapper (`Swarm`)**:
    *   Provides a high-level API (`join`, `leave`, `send`, `on`).
    *   Manages the Node.js Bridge process lifecycle.
    *   Handles data encoding/decoding (Base64/UTF-8).


## Contributing

Contributions are welcome! Please check out the [CONTRIBUTING.md](CONTRIBUTING.md) guide for details.

1.  Fork the project.
2.  Create your feature branch (`git checkout -b feature/AmazingFeature`).
3.  Commit your changes (`git commit -m 'Add some AmazingFeature'`).
4.  Push to the branch (`git push origin feature/AmazingFeature`).
5.  Open a Pull Request.

## License

Distributed under the MIT License. See `LICENSE` for more information.
