# Contributing to KadePy

Thank you for considering contributing to KadePy! This project is open-source and we welcome contributions from the community to make it better, faster, and more secure.

## Getting Started

1.  **Fork the repository** on GitHub.
2.  **Clone your fork** locally:
    ```bash
    git clone https://github.com/ON00dev/KadePy.git
    cd KadePy
    ```
3.  **Create a virtual environment** (recommended):
    ```bash
    python -m venv .venv
    source .venv/bin/activate  # On Windows: .venv\Scripts\activate
    ```
4.  **Install dependencies** and build in editable mode:
    ```bash
    pip install -e .
    ```

## Setting up the Native Environment

KadePy v0.3.0+ uses a hybrid architecture (Python + Native Node.js). To work on the core networking stack:

1.  **Install Node.js & npm**: Ensure they are in your PATH.
2.  **Setup Vendor Dependencies**:
    ```bash
    # The vendor/ directory contains git submodules or cloned repos.
    # Ensure you have them checked out.
    cd vendor/hyperswarm && git checkout v4.16.0
    cd ../hyperdht && git checkout v6.28.0
    # ... check vendor/versions.md for full list
    ```
3.  **Link Dependencies**:
    Go to `kadepy/js` and install/link the dependencies:
    ```bash
    cd kadepy/js
    npm install
    # If using local vendor repos:
    npm link ../../vendor/hyperswarm
    npm link ../../vendor/hyperdht
    # etc.
    ```

## Development Workflow

1.  **Create a branch** for your feature or fix:
    ```bash
    git checkout -b feature/my-new-feature
    ```
2.  **Make your changes**.
    - **Python**: `kadepy/` directory.
    - **Native Bridge**: `kadepy/js/daemon.js` (The JS adapter).
    - **Official Stack**: If you need to patch the official stack, modify the code in `vendor/` and update the reference in `kadepy/js/package.json` or submit PRs upstream.
3.  **Run Tests**:
    - Use the interoperability tests to verify compliance.
    ```bash
    # Run Node.js peer
    cd tests/interop
    node node_peer.js
    
    # Run KadePy peer (in another terminal)
    python python_peer.py
    ```
4.  **Commit your changes**:
    - Use clear and descriptive commit messages.
    ```bash
    git commit -m "Add feature X to improve Y"
    ```
5.  **Push to your fork**:
    ```bash
    git push origin feature/my-new-feature
    ```
6.  **Submit a Pull Request** (PR) to the `main` branch of the original repository.

## Coding Standards

- **Python Code**:
  - Follow PEP 8 style guide.
  - Use type hints where appropriate.
- **JavaScript Code**:
  - Follow standard Node.js async/await patterns.
  - Keep the adapter (`daemon.js`) minimal and logic-free; delegate to `hyperswarm`.

## Reporting Issues

If you find a bug or have a feature request, please open an issue on GitHub. Provide as much detail as possible, including:
- Steps to reproduce the issue.
- Expected behavior.
- Actual behavior.
- Operating System, Python version, and Node.js version.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
