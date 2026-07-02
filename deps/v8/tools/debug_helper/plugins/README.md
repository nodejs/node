# V8 Debug Helper Plugins

This directory contains Python debugger plugins that load
`libv8_debug_helper` to facilitate post-mortem and live debugging of V8
frames and objects in GDB and LLDB.

## How To Build It

Build the shared debug helper library from the repository root:

```sh
autoninja -C out/<config> v8_debug_helper_shared
```

The plugins load the library from the `V8_DEBUG_HELPER_LIB_PATH`
environment variable.

## How To Use It

These plugins are only tested to work on x64 and arm64 Linux and arm64 macOS,
where the debuggers support at least Python 3.9+.

First set `V8_DEBUG_HELPER_LIB_PATH` environment variable to the built shared
library.

For GDB, run the following commands:

```sh
source tools/gdbinit
source tools/debug_helper/plugins/gdb_plugin.py
```

For LLDB:

```sh
command script import tools/debug_helper/plugins/lldb_plugin.py
```

The LLDB plugin installs its own `frame-format` setting for the session. Import
it in a debugger session where that takeover is acceptable.

### Current features

Once loaded, the plugins append JavaScript annotations to candidate V8 frames
when you print a backtrace via `bt`, and adds a `v8` command. Currently, `v8` supports the following subcommands:

```
v8 inspect <addr> [--type T] [--depth N] [--array-length N]
```

#### Frame annotations

The annotation format for frames displayed in `bt` is:

```
[<function_name> @ <script_name>:<line>:<column>] (this=0xADDR, argc=N)
```

The trailing `(this=0xADDR, argc=N)` shows the receiver tagged-pointer and
argument count for the JS frame. If the frame's slots are unreadable, the
trailer is dropped.

If source text cannot be recovered but the script name still can, the
annotation degrades to:

```
[<function_name> @ <script_name>]
```

For anonymous functions the name is shown as `<anonymous>`. The line and column
point to the start of the function scope in its definition, which is
normally the `(` of the parameter list (or position 1:1 for the top-level
script scope), not where the function is called, which we cannot
reliably recover in the debugger.

#### `v8 inspect <addr>`

Inspects a tagged V8 object at `<addr>` and prints its properties.

```
(gdb) v8 inspect 0x34f49880471
0x34f49880471 <JSArray: length=3>
  .map=0x34f49880409 <Map: for JSArray>
  .properties_or_hash=0x34f49880421 <FixedArray: empty>
  .elements=0x34f49880491 <FixedArray: length=3>
  .length=<Smi: 3>
```

Options:

| Flag | Effect |
|---|---|
| `--type <T>` | Type hint. Useful when the Map is unreadable (e.g. `v8::internal::JSArray`). |
| `--depth N` | Inline-recursion depth for child inspection (default: 1). |
| `--array-length N` | Per-array element cap (default: 16). |

When the object's Map can't be read (e.g. due to memory corruption), the renderer adds
a `could be one of ...` footer with ready-to-paste `--type` suggestions.

#### Examples

To display the JS frames on stack, and inspect the receiver, paste the address
in the frame annotation into `v8 inspect`:

```
(gdb) bt
...
#5 0x... in Builtins_InterpreterEntryTrampoline [test_func_3 @ throw.js:15:21] (this=0x1d190100497d, argc=4)
...
(gdb) v8 inspect 0x1d190100497d
0x1d190100497d <JSGlobalProxy>
  .map=0x1d190101c3ed <Map>
  .properties_or_hash=<Smi: 940196>
  .elements=0x1d19000007e5 <FixedArray: EmptyFixedArray>
  .cpp_heap_wrappable=0x00000000
```

## How To Test It

Before running the debugger tests, build the test dependencies:

```sh
autoninja -C out/<config> d8 v8_debug_helper_shared corruption_harness
```

There are several types of targets in the Makefile:

- `prepare-cores` generates core files from the test scripts for reuse in
  `run-core-*` and the core `test_*` targets. The files are saved to
  `CORE_DIR`, which defaults to `$(OUT_DIR)/debug_helper.cores/` and is kept
  out of source control.
- `run-live-*` targets run a debugging session on a test script in
  non-interactive mode and print the output. This is useful for debugging
  the plugins themselves.
  - `run-core-*` targets run the same sessions but load from a prepared core
    file instead of a live process.
- `test_<module>` targets run one unittest module and assert the expected
  output, e.g. `test_gdb_live` or `test_lldb_core`. Append a class to run a
  single suite, e.g. `test_gdb_core.GdbInspectCoreTest`.
  - Modules whose name contains `core` load from prepared cores and depend on `prepare-cores`.
- Convenience suites that run several tests at once:
  - `test-live` runs the live tests on both debuggers.
  - `test-core` runs the core-file tests on both debuggers.
  - `test-gdb` / `test-lldb` run both tests for one debugger.
  - `test` runs everything.

On macOS, with `lldb` installed globally, the output directory is
assumed to be `out/arm64.release`. Build and run the tests with:

```sh
autoninja -C out/arm64.release d8 v8_debug_helper_shared corruption_harness
make -C tools/debug_helper/plugins test_lldb_live
# To run core tests, first prepare cores, this can take a while
make -C tools/debug_helper/plugins prepare-cores
make -C tools/debug_helper/plugins test_lldb_core
```

On Linux, with `gdb` and `lldb` installed, the output directory is assumed
to be `out/x64.release`. Build and run the tests with:

```sh
autoninja -C out/x64.release d8 v8_debug_helper_shared corruption_harness
make -C tools/debug_helper/plugins test-live
# To run core tests, first prepare cores, this can take a while
make -C tools/debug_helper/plugins prepare-cores
make -C tools/debug_helper/plugins test-core
```

If the output directory is different from the assumed one,
set the `OUT_DIR` environment variable when running the Makefile.

```sh
OUT_DIR="$(pwd)/out/x64.release.test" make -C tools/debug_helper/plugins test_lldb_live
```

## Development Notes

Set `V8_DEBUG_HELPER_VERBOSE=1` to enable verbose logging in the plugins.

To see the raw output from the debugger without assertions, use the `run-*`
targets instead of `test-*`, for example:

```sh
make -C tools/debug_helper/plugins run-live-backtrace-lldb
```

## Directory Layout

- `gdb_plugin.py`: GDB plugin entry point.
- `lldb_plugin.py`: LLDB plugin entry point.
- `v8dbg/`: shared plugin code.
- `test/`
  - `fixtures/`: test scripts and expected annotation fixtures.
  - `helpers/`: Python helpers for the tests
  - `test_*.py`: Unit tests. Run them through the Makefile (see "How To Test It").

## Design

`libv8_debug_helper` is the dynamic library built by `v8_debug_helper_shared`,
which exposes C APIs whose definitions can be found in
`debug_helper.h`. The `DebuggerBridge` class in `v8dbg/shared_bridge.py`
wraps those APIs in Python via `ctypes` for the debugger plugins
to call into.

```text
  GDB / LLDB UI
    |
    v
  GDB/LLDB Python plugins
    |
    v
  DebuggerBridge API
    |
    v
  libv8_debug_helper
    |
    v
  DebuggerBridge memory accessor callback
    |
    v
  live process or core dump memory
```

See the `DebuggerBridge` method docstrings in `v8dbg/shared_bridge.py` for the
current API.
