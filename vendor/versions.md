# KadePy Vendor Dependencies

KadePy uses the official Hyperswarm stack via native bindings.
The following repositories are vendored in this directory and pinned to specific versions to ensure stability and compatibility.

## Pinned Versions

| Component | Version | Commit | Description |
|-----------|---------|--------|-------------|
| **hyperswarm** | v4.16.0 | `ea38752` | High-level swarm networking. Chosen as the latest stable v4 release compatible with current ecosystem. |
| **hyperdht** | v6.28.0 | `3f1004d` | DHT implementation. Pinned to match Hyperswarm v4 requirements. |
| **dht-rpc** | v3.4.1 | `ea127f8` | RPC layer for DHT. Compatible with HyperDHT v6. |
| **libudx** | v1.6.9 | `4397d3b` | Native UDP transport (C library). Stable release used by udx-native. |
| **udx-native** | v1.9.0 | `dc4a5e5` | Node.js bindings for libudx. Matches libudx v1.6.x. |

## Rationale

These versions are chosen based on the standard Node.js Hyperswarm stack as of late 2024.
- **hyperswarm v4** is the widely used version.
- **hyperdht v6** is the corresponding DHT version.
- **libudx/udx-native** provide the reliable UDP transport required by the stack.

## Update Policy

To update dependencies:
1. `cd vendor/<repo>`
2. `git fetch`
3. `git checkout <new-tag>`
4. Update this file.
5. Re-verify interop tests.
