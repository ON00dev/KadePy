# KadePy Protocol Specification (v0.2)

This document describes the binary protocol used by KadePy nodes to communicate.

## 1. Fundamentals

- **Transport**: UDP
- **Byte Order**: Network Byte Order (Big-Endian) for all multi-byte integers (IPs, Ports).
- **Node ID**: 32 bytes (256 bits).
- **Distance Metric**: XOR (Kademlia metric).
- **Authentication**: Ed25519 Signatures.

## 2. Packet Structure

Every packet consists of a **Header** followed by an optional **Payload**.

### 2.1. Header (105 bytes)

| Offset | Size | Field | Description |
| :--- | :--- | :--- | :--- |
| 0 | 1 | `Type` | Message Type ID |
| 1 | 32 | `SenderPublicKey` | The 256-bit Ed25519 Public Key of the sender (also acts as Node ID) |
| 33 | 8 | `Timestamp` | 64-bit Unix Timestamp (milliseconds) |
| 41 | 64 | `Signature` | Ed25519 Signature of the packet content |

### 2.2. Message Types

| Value | Name | Payload | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `PING` | *None* | Keep-alive check. |
| `0x01` | `PONG` | *None* | Response to PING. |
| `0x02` | `FIND_NODE` | `TargetID` (32B) | Request closest nodes to TargetID. |
| `0x03` | `FOUND_NODES`| `Count` (1B) + `Nodes` | Response to FIND_NODE. |
| `0x04` | `ANNOUNCE` | `InfoHash` (32B) + `Port` (2B)| Announce availability of a resource (peer). |
| `0x05` | `GET_PEERS` | `InfoHash` (32B) | Request peers for a specific resource. |
| `0x06` | `PEERS` | `InfoHash` (32B) + `Count` (1B) + `Peers` | Response to GET_PEERS. |

## 3. Payload Formats

### 3.1. Node Wire Format (38 bytes)
Used in `FOUND_NODES` responses to transmit contact info of K nodes.

| Size | Field | Description |
| :--- | :--- | :--- |
| 32 | `ID` | Node ID |
| 4 | `IP` | IPv4 Address (Big Endian) |
| 2 | `Port` | UDP Port (Big Endian) |

### 3.2. Peer Wire Format (6 bytes)
Used in `PEERS` responses to transmit peer contact info (IP/Port only).

| Size | Field | Description |
| :--- | :--- | :--- |
| 4 | `IP` | IPv4 Address (Big Endian) |
| 2 | `Port` | UDP Port (Big Endian) |

## 4. Encryption (Optional)

If a **Network Key** is set, the packet payload (everything after the Header) can be encrypted using **ChaCha20**.

- **Nonce**: Derived from timestamp or packet counter (implementation specific detail, currently standard implementation uses a static or derived nonce convention which is subject to future revision).
- **Key**: 32-byte shared secret.

## 5. Routing Table

- **K-Buckets**: Nodes are stored in buckets based on the XOR distance from the local node.
- **K**: Typically 20 (max nodes per bucket).
- **Bucket Splitting**: Standard Kademlia splitting rules apply.

## 6. Hyperswarm Native Protocol (v0.2+)

The Native Hyperswarm Extension implements a distinct protocol stack compatible with Hyperswarm's UDX and Noise layer.

### 6.1. UDX Packet Structure

UDX packets are packed (`#pragma pack(1)`) and structured as follows:

| Size | Field | Description |
| :--- | :--- | :--- |
| 1 | `Type` | Packet type (e.g., Data=1, Ack=2, Syn=3) |
| 4 | `ConnID` | Connection ID (Little Endian) |
| 4 | `Seq` | Sequence Number (Little Endian) |
| 4 | `Ack` | Acknowledgment Number (Little Endian) |
| Var | `Payload` | Data payload (Encrypted after handshake) |

### 6.2. Noise Handshake (XX)

Authentication and Key Exchange follow the Noise XX pattern:
1.  **Msg 1 (A->B)**: `e` (Ephemeral Key)
2.  **Msg 2 (B->A)**: `e, ee, s, es` (Responder Auth)
3.  **Msg 3 (A->B)**: `s, se` (Initiator Auth)

After the handshake, transport keys are derived for split-mode encryption (Initiator Tx = K1, Rx = K2).
