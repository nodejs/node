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

| Platform | Supported toolchains | Driver                                |
| -------- | -------------------- | ------------------------------------- |
| Windows  | Clang-CL             | `vcbuild.bat` + `pgo.py`                |
| Linux    | GCC, Clang           | `configure` + `make`, `pgo.py` for Clang |
| macOS    | Clang                | `configure` + `make` + `pgo.py`         |

The two supported flows differ in how profile data is collected. Clang writes
one `.profraw` file per process, which must be merged into a single
`.profdata` before the optimize phase. GCC's libgcov instead merges counters
into `.gcda` files next to each object file as each process exits, so there is
no merge step.

## Quick Start: Windows

From a VS Developer Command Prompt, at the repo root:

```powershell
# Step 1: Build the instrumented binary
vcbuild.bat pgo-generate

# Step 2: Run workloads to collect profile data
python tools\pgo\pgo.py

# Step 3: Build the optimized binary
vcbuild.bat pgo-use
```

`pgo.py` expects the instrumented binary at `Release\node.exe` (produced by
step 1) and writes `node.profdata` to the repo root (consumed by step 3).

The script finds `llvm-profdata` in the Visual Studio LLVM toolset, then
`PATH`. Set `LLVM_PROFDATA` to the matching tool when using a different
Clang installation.

```powershell
# Optionally set a longer training duration (default: 15s per script)
python tools\pgo\pgo.py --duration=30
```

### GitHub Actions: Windows PGO profdata generation

The [Windows PGO workflow](../../.github/workflows/pgo-windows.yml)
is manually dispatched and only runs in `JaneaSystems/node`. Once it is on the
fork's default branch, open
**Actions > Windows PGO profdata generation > Run workflow** and select the
branch containing the workflow definition. Set `source_ref` to a full commit
SHA, branch, or tag to fetch from `JaneaSystems/node`; this input does not select
another fork. The repository condition checks where the workflow runs, not the
origin of the selected commit. Choose the target `architecture`: `arm64`
(the default) or `x64`. The selected source commit does not need to contain the
workflow file. Supply `committer_name` and `committer_email` with your full name
and email for the commit identity and `Signed-off-by` trailer, attesting to the
[Developer Certificate of Origin](https://developercertificate.org/).

The workflow builds with
`vcbuild.bat build-release <architecture> pgo-generate nosign` on `windows-2025`.
The `nosign` option avoids the Azure Trusted Signing credentials otherwise
required by `build-release`. The source ref is resolved once at checkout, and
every job uses that exact commit even if the source branch moves during the run.
The selected revision must support the Windows PGO build command and contain
`tools/pgo/pgo.py`; revisions predating that support cannot use this workflow.
The workflow sets up Python 3.15 and Rust 1.88, adds the
`aarch64-pc-windows-msvc` Rust target for ARM64, and installs NASM for x64.

For ARM64, the workflow transfers `Release/node.exe` and the matching
`llvm-profdata` tool to `windows-11-arm` for training. Only the x64 profile merge
tool runs under Windows emulation; Node.js and the workloads run natively on
ARM64. For x64, `pgo.py` runs directly after compilation in the same job on the
same machine. The separate ARM64 training job and binary transfer are skipped.

After `pgo.py` succeeds, the workflow places `node.profdata` in
`tools/pgo/profdata/windows/<architecture>/` and pushes a new branch named
`pgo-windows-<architecture>-<source_sha>-YYYY-MM-DD` to `JaneaSystems/node`, using
the full source commit SHA and its committer date converted to UTC. Immediately
after resolving the source commit, the build job checks whether this branch
exists, before setting up tools or compiling. If it does, the build job fails
and reports that the action was already done previously; training and publication
are skipped. The generated commit is based on that source commit.
Its subject is `win,build: add <architecture> pgo profdata`, with
your sign-off and an `Assisted-by: GitHub Copilot` trailer. The downloaded profile
replaces any existing file, and both new and modified profiles are staged. The
workflow commits and pushes even if the profile is unchanged, allowing an empty
commit in that case.

Only the publication job receives `contents: write`, using the fork's
`GITHUB_TOKEN`; no personal access token is needed. Review the generated commit
before using it in a contribution. Profiles are retained as workflow artifacts
for seven days. ARM64 runs also retain the instrumented build transferred to the
training runner.

## Quick Start: Linux with GCC

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

## Quick Start: Linux and macOS with Clang

From the repo root:

```bash
# Step 1: Build the instrumented binary
./configure --ninja --enable-pgo-generate
make

# Step 2: Run workloads to collect profile data
python3 tools/pgo/pgo.py

# Step 3: Build the optimized binary
./configure --ninja --enable-pgo-use
make
```

`pgo.py` expects the instrumented binary at `out/Release/node` (produced by
step 1) and writes `node.profdata` to the repo root (consumed by step 3).

The script finds `llvm-profdata` through `xcrun` on macOS and `PATH` on Linux.
Set `LLVM_PROFDATA` to the matching tool when using a different Clang
installation.

```bash
# Optionally set a longer training duration (default: 15s per script)
python3 tools/pgo/pgo.py --duration=30
```

## Clang Profile Collection

On all platforms, `pgo.py` collects raw profiles in a fresh directory and
replaces `node.profdata` after a successful merge. It removes raw profiles
after success and preserves them if training or merging fails. Training
failures stop the script so the workloads can be fixed before trying again.

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
workloads). When used with `pgo.py`, this is handled automatically.

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
├── pgo.py                  # Clang training driver (collect + merge)
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
