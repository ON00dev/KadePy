# Roadmap

This document outlines the development plan for KadePy.

## ✅ Version 0.1.0 (Alpha)
- **Core DHT**: Implementation of `PING`, `FIND_NODE`, `ANNOUNCE_PEER`, `GET_PEERS`.
- **Hybrid Architecture**: C Extension for performance, Python for API.
- **Cross-Platform**: Full support for Windows (Winsock/BCrypt) and Linux (POSIX).
- **Security**: Cryptographically secure RNG for ID generation.
- **Performance**: GIL-optimized reactor thread.

## ✅ Version 0.2.0 (Beta) - Current
- **Hyperswarm Native Extension**:
    - [x] Initial C extension structure (`_hyperswarm`).
    - [x] Noise Protocol Handshake (C implementation w/ libsodium).
    - [x] UDX Transport Layer (Reliable UDP structure).
    - [x] Distributed Holepunching (Logic flow).
- **Storage RPCs**: Implement `STORE` and `FIND_VALUE` to allow storing generic data (blobs) in the DHT, not just peer contacts.

- **Improved Bootstrapping**: Better logic for joining the network and refreshing buckets.
- **Error Handling**: richer exception types in Python for C-level network errors.
- **CI/CD**: GitHub Actions for automated building and testing of wheels.

## 🛠 Version 0.3.0 (Stable Candidate)
- **IPv6 Support**: Update `udp_reactor.c` and protocol structs to support IPv6 addresses (16 bytes).
- **NAT Traversal**:
    - STUN client integration.
    - UPnP support (MiniUPnPc).
- **Rate Limiting**: Token bucket algorithm in C to prevent packet flooding/DoS.
- **AsyncIO Support**: Integration with Python's `asyncio` event loop.

## 🔮 Version 1.0.0 (Production)
- **Protocol Audit**: Formal verification of the protocol specification.
- **Encryption Suite**: Full Handshake and Session Key exchange (Noise Protocol or similar) instead of pre-shared keys.
- **Massive Scale**: Validated with simulations of 10,000+ nodes.
- **Plugin System**: Hooks for custom message types.
