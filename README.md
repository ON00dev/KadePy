# KadePy

**High-Performance P2P Swarm Implementation via Official Hyperswarm Bindings**

KadePy is a robust and efficient library for connecting to the Hyperswarm network. It leverages the official, battle-tested Hyperswarm stack (Node.js) through a high-performance native binding, ensuring 100% protocol compliance, reliable holepunching, and secure communications.

## Features

- **Official Hyperswarm Stack**: Powered by `hyperswarm`, `hyperdht`, `dht-rpc`, and `libudx`.
- **Full Compatibility**: Guaranteed wire compatibility with any Hyperswarm peer (Node.js, Holepunch, etc.).
- **Native Bindings**:
  - Uses a lightweight, managed Node.js daemon for protocol handling.
  - Transparent IPC bridge for high-performance communication between Python and the swarm.
- **Secure**:
  - **Noise Handshake (XX Pattern)**: Authenticated, encrypted connections using `libsodium` (Ed25519/Curve25519).
  - **UDX Transport**: Reliable, congestion-controlled UDP transport.
- **Easy to Use**: Simple Python API for joining topics, finding peers, and exchanging data.
- **Zero-Config**: Automatically handles dependency management and process lifecycle.

## Installation

### Requirements

- Python 3.10+
- Node.js & npm (Required for the underlying native bindings)

### From Source

```bash
git clone https://github.com/ON00dev/KadePy.git
cd KadePy
pip install .
```

## Quick Start

### Basic Swarm Node

```python
from kadepy import Swarm
import hashlib
import time

# Create a node (automatically initializes the native backend)
node = Swarm()
print("Node started!")

# Generate a topic key (32-byte hex string or bytes)
topic = hashlib.sha256(b"my-app-topic").digest().hex()
print(f"Joining topic: {topic}")

# Join the swarm for this topic
node.join(topic)

# Define callback for new connections
def on_connection(socket, info):
    print(f"New connection! Peer Key: {info.get('publicKey')}")
    
    # Send a greeting
    socket.sendall(b"Hello from KadePy!")
    
    # Receive data in a loop (simple example)
    while True:
        data = socket.recv(1024)
        if not data: break
        print(f"Received: {data.decode()}")

# Register the callback
node.on("connection", on_connection)

# Keep the process alive
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Stopping...")
    node.close()
```

## Interoperability Example

KadePy is fully compatible with standard Node.js Hyperswarm peers.

### 1. Node.js Peer (`peer.js`)

First, install hyperswarm: `npm install hyperswarm`

```javascript
const Hyperswarm = require('hyperswarm')
const crypto = require('crypto')

const swarm = new Hyperswarm()
// Use the same topic as Python
const topic = crypto.createHash('sha256').update('my-interop-topic').digest()

swarm.join(topic)

swarm.on('connection', (socket) => {
  console.log('New connection from KadePy!')
  socket.write('Hello from Node.js')
  socket.on('data', data => console.log('Received:', data.toString()))
})
```

### 2. Python Peer (`peer.py`)

```python
from kadepy import Swarm
import hashlib
import time

node = Swarm()
# Use the same topic as Node.js
topic = hashlib.sha256(b"my-interop-topic").digest().hex()

node.join(topic)

def on_connection(socket, info):
    print("Connected to Node.js peer!")
    # Send message to Node.js
    socket.sendall(b"Hello from Python")
    # Receive message from Node.js
    print("Received:", socket.recv(1024).decode())

node.on("connection", on_connection)

# Keep alive
try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    node.close()
```

## Architecture

KadePy v0.3.0 adopts a "Native Binding" architecture:

1.  **Python Frontend**: Provides the idiomatic `Swarm` class and event handling.
2.  **Native Daemon**: A pinned, vendored version of the official Hyperswarm stack running as a managed subprocess.
3.  **IPC Bridge**: High-speed local TCP bridge for stream piping, allowing Python to interact with raw UDX streams as if they were local sockets.

This architecture ensures that KadePy behaves *exactly* like a native Node.js Hyperswarm peer on the network layer, eliminating protocol drift and implementation bugs.

## Contributing

Contributions are welcome! Please check out the [CONTRIBUTING.md](CONTRIBUTING.md) guide for details.

## License

Distributed under the MIT License. See `LICENSE` for more information.
