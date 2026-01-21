# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-01-20

### Added
- Initial release of KadePy.
- **Core**: High-performance C extension for DHT logic (Routing, Protocol, Storage).
- **Network**: Non-blocking UDP reactor with threading support.
- **Python API**: `Swarm` class for easy integration.
- **Security**:
  - Secure Random Number Generator (RNG) using `BCryptGenRandom` (Windows) and `/dev/urandom` (POSIX).
  - Optional ChaCha20 packet encryption.
- **Cross-Platform**: Full support for Windows, Linux, and macOS.
- **Optimization**: GIL optimization to allow parallel execution of C core.

### Fixed
- Fixed network byte order issues (Endianness) ensuring correct IP/Port parsing across platforms.
- Fixed compiler warnings and errors for smoother build process.
