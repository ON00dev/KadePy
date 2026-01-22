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

## Development Workflow

1.  **Create a branch** for your feature or fix:
    ```bash
    git checkout -b feature/my-new-feature
    ```
2.  **Make your changes**.
    - If modifying C code (`src/`), ensure it compiles without warnings.
    - If working on the **Native Hyperswarm Extension**, ensure you have `libsodium` available or in `vendor/libsodium`.
    - If modifying Python code (`kadepy/`), follow standard Python coding conventions (PEP 8).
3.  **Run Tests**:
    - We currently have `verify_full.py` and `test_script.py` for integration testing.
    - Ensure all tests pass before committing.
    ```bash
    python setup.py build_ext --inplace  # Rebuild C extension
    python tests/verify_native_crypto.py # Verify encryption/handshake
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

- **C Code**:
  - Keep it ANSI C compliant where possible.
  - Use `stdint.h` types (`uint32_t`, `uint8_t`, etc.) for fixed-width integers.
  - Manage memory carefully (malloc/free).
  - Use `#ifdef _WIN32` for platform-specific code.
- **Python Code**:
  - Follow PEP 8 style guide.
  - Use type hints where appropriate.

## Reporting Issues

If you find a bug or have a feature request, please open an issue on GitHub. Provide as much detail as possible, including:
- Steps to reproduce the issue.
- Expected behavior.
- Actual behavior.
- Operating System and Python version.

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
