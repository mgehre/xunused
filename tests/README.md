# xunused Test Suite

This directory contains LLVM-style lit tests for xunused.

## Running Tests Locally

### Prerequisites

You need the following tools installed:
- **LLVM package**: Provides `FileCheck` and `lit` testing infrastructure
  - On Ubuntu/Debian: `sudo apt-get install llvm`
- **xunused binary**: Build the project first with `cmake` and `make`

### Running the Tests

1. **Build xunused**:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

2. **Add xunused to PATH**:
   ```bash
   export PATH="$(pwd)/build:$PATH"
   ```

3. **Set up llvm-lit** (if not already available):
   ```bash
   # Find and use the lit.py script from your LLVM installation
   LIT_PATH=$(find /usr/lib/llvm-*/build/utils/lit/lit.py -print -quit 2>/dev/null)
   if [ -n "$LIT_PATH" ]; then
     alias llvm-lit="python3 $LIT_PATH"
   fi
   ```

4. **Run the tests**:
   ```bash
   llvm-lit -v tests
   ```

## Test Structure

- `lit.cfg.py`: Configuration file for the lit test runner
- Test files (`.test`, `.c`, `.cpp`, `.sh`) in the `tests/` directory
- Each test file uses LLVM-style `RUN:` and `CHECK:` directives

## Writing Tests

Tests use the LLVM FileCheck format:
- `RUN:` lines specify commands to execute
- `CHECK:` lines specify patterns to match in the output
- `%t` is a temporary directory for test artifacts
- `split-file` can be used to embed multiple input files in a single test

See `basic.test` for an example using `split-file`.
