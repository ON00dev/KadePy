# KadePy Architecture

KadePy is a high-performance, distributed hash table (DHT) library that leverages a hybrid architecture: a robust C core for networking and protocol logic, and a flexible Python layer for application-level APIs.

## 1. High-Level Overview

```mermaid
graph TD
    UserApp[User Python Application] -->|Uses| Swarm[kadepy.Swarm Class]
    Swarm -->|Calls| CExt[C Extension (_core)]
    
    subgraph "C Core (Native Performance)"
        CExt -->|Manages| Reactor[UDP Reactor Thread]
        Reactor -->|Recv/Send| Socket[UDP Socket]
        Reactor -->|Parses| Protocol[Protocol Handler]
        Protocol -->|Updates| Routing[Routing Table (K-Buckets)]
        Protocol -->|Secure RNG| Random[Crypto/RNG]
    end
    
    Reactor -->|Callbacks| Swarm
```

## 2. Python Layer (`kadepy/`)

The Python layer is designed to be idiomatic and easy to use.

- **`Swarm` Class**: The main entry point. It manages the lifecycle of the DHT node, handles bootstrapping, and exposes methods like `find_node`, `announce_peer`, and `get_peers`.
- **Callback System**: The Python layer registers a global callback function with the C core. When the C core receives relevant messages (e.g., `MSG_FOUND_NODES`), it acquires the Global Interpreter Lock (GIL) briefly to execute this callback, passing the data back to Python.

## 3. C Core (`src/`)

The C core handles the heavy lifting to ensure low latency and high throughput.

### 3.1. Directory Structure
- **`kadepy_module.c`**: The bridge between Python and C. It defines the `_core` module, parses arguments, and converts C types to Python objects.
- **`net/udp_reactor.c`**: Implements the networking loop.
    - Uses `select` (or platform specific polling) to handle I/O.
    - runs in a dedicated background thread (`pthread` on POSIX, `CreateThread` on Windows).
    - **GIL Optimization**: The reactor thread runs *without* holding the GIL most of the time. It only acquires the GIL when it needs to invoke the Python callback.
- **`dht/protocol.c`**: Handles packet serialization/deserialization and protocol logic (state machine).
- **`dht/routing.c`**: Implements the K-Bucket routing table algorithms (XOR distance, bucket splitting, peer replacement).
- **`utils/random.c`**: Provides cryptographically secure random numbers using `BCryptGenRandom` (Windows) or `/dev/urandom` (POSIX).

### 3.2. Concurrency Model

KadePy uses a **Threaded Reactor** model:

1.  **Main Thread (Python)**: Executes user code and makes non-blocking calls to the C extension to send packets.
2.  **Reactor Thread (C)**: loops indefinitely, waiting for UDP packets.
    - **Incoming Packet**:
        1.  Socket receives data.
        2.  Protocol parser validates the header.
        3.  Routing table is updated (sender seen).
        4.  If the packet corresponds to a pending request or is an event the user cares about, the **GIL is acquired**.
        5.  Python callback is executed.
        6.  **GIL is released**.
    - **Outgoing Packet**:
        - Sent immediately from the calling thread (thread-safe socket usage).

### 3.3. Security & Cryptography

- **Node IDs**: 256-bit identifiers (32 bytes).
- **RNG**: OS-provided CSPRNG is used for generating IDs and transaction tokens.
- **Encryption (Optional)**: The architecture supports ChaCha20 for encrypting packet payloads if a network key is configured.

## 4. Cross-Platform Support

The codebase uses conditional compilation (`#ifdef _WIN32`) to handle OS differences:
- **Networking**: `Winsock2` vs `sys/socket.h`.
- **Threading**: `Windows API` vs `pthreads`.
- **RNG**: `BCrypt` vs `/dev/urandom`.
