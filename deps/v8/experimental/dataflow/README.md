# V8 C++ Dataflow Analyzer

Static C++ dataflow analyzer and interactive explorer built on Clang LibTooling, SQLite, and a zero-copy `mmap` CSR/CSC index.

## Building

Requires CMake, SQLite3, and LLVM/Clang 19+ (`$LLVM_DIR`, `~/external/llvm/install`, or `/usr/lib/llvm-19`).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(nproc)
```

## Running Unit Tests

```bash
python3 tests/run_tests.py
```

## Indexing V8

Generate `compile_commands.json` for your V8 build directory (for example `out/x64.release`) and index the codebase:

```bash
./build/analyzer --index -j $(nproc) \
  --workspace-root=/path/to/v8 \
  --cache-dir=/path/to/v8/.cache \
  --compile-commands-dir=/path/to/v8/out/x64.release
```

## Launching the WebUI

```bash
python3 webui/server.py \
  --workspace-root=/path/to/v8 \
  --cache-dir=/path/to/v8/.cache \
  --analyzer=./build/analyzer \
  --port 8765
```
