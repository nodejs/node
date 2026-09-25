# Node.js PGO Training Scripts

Training workloads for Profile-Guided Optimization (PGO) builds.

## What is PGO?

PGO uses runtime profile data to guide compiler optimizations (inlining,
branch prediction, code layout), typically improving throughput by 5-20%.

The process has three phases:

1. **Instrument** — Build with `-fprofile-generate`
2. **Train** — Run representative workloads to collect profile data
3. **Optimize** — Rebuild with `-fprofile-use`

## Platform Support

| Platform | Supported toolchains | Driver                    |
| -------- | -------------------- | ------------------------- |
| Windows  | Clang-CL             | `vcbuild.bat` + `pgo.ps1` |
| Linux    | GCC                  | `configure` + `make`      |
| macOS    | —                    | —                         |

The two supported flows differ in how profile data is collected. Clang writes
one `.profraw` file per process, which must be merged into a single
`.profdata` before the optimize phase. GCC's libgcov instead merges counters
into `.gcda` files next to each object file as each process exits, so there is
no merge step.

Clang on Linux and macOS are not supported yet.

## Quick Start: Windows

From a VS Developer Command Prompt, at the repo root:

```powershell
# Step 1: Build the instrumented binary
vcbuild.bat pgo-generate

# Step 2: Run workloads to collect profile data
powershell -ExecutionPolicy Bypass -File .\tools\pgo\pgo.ps1

# Step 3: Build the optimized binary
vcbuild.bat pgo-use
```

`pgo.ps1` expects the instrumented binary at `Release\node.exe` (produced by
step 1) and writes `node.profdata` to the repo root (consumed by step 3).

The script is unsigned, so the default execution policy refuses to run it
without `-ExecutionPolicy Bypass`. Use `pwsh` in place of `powershell` on
PowerShell 7.

```powershell
# Optionally set a longer training duration (default: 15s per script)
powershell -ExecutionPolicy Bypass -File .\tools\pgo\pgo.ps1 -Duration 30
```

## Quick Start: Linux

```bash
# Step 1: Build the instrumented binary
./configure --enable-pgo-generate
make

# Step 2: Run workloads to collect profile data
./out/Release/node tools/pgo/pgo-run-all.js --duration=15 --verbose

# Step 3: Build the optimized binary
./configure --enable-pgo-use
make
```

Step 2 needs no driver script. Each object file gets one counter file beside
it, with the same basename and a `.gcda` extension:

```text
out/Release/obj/src/node_base.node_binding.o    # from step 1
out/Release/obj/src/node_base.node_binding.gcda # from step 2
```

Keep `out/` intact between steps 1 and 3. GCC records the `.gcda` path into
each object at compile time, so `make clean` or `make distclean` discards the
training data and step 3 silently produces an ordinary build.

The build passes `-fprofile-correction`, which is required here. Counter
updates from the worker threads and the libuv thread pool race with each
other, and GCC treats the resulting inconsistent profile as an error unless
told to smooth it out.

## Training Scripts

All scripts use only Node.js built-in modules (no npm dependencies).
Each script is run as a separate process via `fork()`.

| Script                   | What it exercises                                             |
| ------------------------ | ------------------------------------------------------------- |
| `pgo-http-server.js`     | llhttp parser, TCP stack, header serialization, JSON, routing |
| `pgo-json.js`            | V8 JSON parser/serializer, string allocation, GC pressure     |
| `pgo-crypto.js`          | OpenSSL (hashing, HMAC, AES, RSA, ECDSA, random, KDF)        |
| `pgo-streams-buffers.js` | Buffer C++ impl, stream state machine, back-pressure          |
| `pgo-fs.js`              | libuv fs operations, thread pool, path module                 |
| `pgo-async-patterns.js`  | V8 Promises, microtask queue, EventEmitter, timers            |
| `pgo-url-string.js`      | Ada URL parser, V8 string internals, regex JIT                |
| `pgo-compression.js`     | zlib, brotli C libraries, streaming compression               |
| `pgo-net.js`             | libuv TCP/pipe handles, c-ares DNS resolver                   |
| `pgo-module-loading.js`  | Module resolver, V8 script compilation, vm module             |
| `pgo-child-workers.js`   | Worker thread messaging, SharedArrayBuffer, inline eval       |

### Running the Orchestrator Directly

The orchestrator can also be invoked directly (e.g. for testing individual
workloads). When used with `pgo.ps1`, this is handled automatically.

```bash
# Run all scripts
./out/Release/node tools/pgo/pgo-run-all.js --duration=15 --verbose

# Run specific scripts
./out/Release/node tools/pgo/pgo-run-all.js --scripts=http-server,json,crypto --duration=30

# Show help
./out/Release/node tools/pgo/pgo-run-all.js --help
```

Each script reads the `PGO_TRAINING_DURATION` environment variable (in
milliseconds) to determine how long to run. The orchestrator sets this
automatically from the `--duration` flag (in seconds).

## Files

```
tools/pgo/
├── pgo.ps1                 # Windows training driver (collect + merge)
├── pgo-run-all.js          # Training orchestrator
├── pgo-http-server.js      # HTTP server + client workload
├── pgo-json.js             # JSON parse/stringify workload
├── pgo-crypto.js           # Crypto operations workload
├── pgo-streams-buffers.js  # Streams and Buffer workload
├── pgo-fs.js               # File system operations workload
├── pgo-async-patterns.js   # Promise/async, EventEmitter, timers workload
├── pgo-url-string.js       # URL parsing, string ops, regex workload
├── pgo-compression.js      # Gzip/brotli/deflate compression workload
├── pgo-net.js              # TCP networking and DNS workload
├── pgo-module-loading.js   # Module require/import, VM compilation workload
├── pgo-child-workers.js    # Worker threads workload
└── README.md               # This file
```
