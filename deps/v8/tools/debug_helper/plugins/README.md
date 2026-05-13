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

The plugins currently only annotate V8 frames in backtraces, but the bridge can
support more features later.

Once loaded, the plugins append JavaScript annotations to candidate V8 frames
when you print a backtrace via `bt`. The annotation format is:

```
[<function_name> @ <script_name>:<line>:<column>]
```

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

## How To Test It

Before running the debugger tests, build the test dependencies:

```sh
autoninja -C out/<config> d8 v8_debug_helper_shared corruption_harness
```

There are several types of targets in the Makefile:

- `prepare-cores` generates core files from the test scripts for reuse in
  `run-core-*` and `test-core-*` targets. The files are saved to `CORE_DIR`,
  which defaults to `$(OUT_DIR)/debug_helper.cores/` and is kept out of
  source control.
- `run-live-*` targets run a debugging session on a test script in
  non-interactive mode and print the output. This is useful for debugging
  the plugins themselves.
  - `run-core-*` targets run the same sessions but load from a prepared core
    file instead of a live process.
- `test-live-*` targets run the same live sessions but also assert the
  expected annotations in Python.
  - `test-core-*` targets run the same assertions but against the prepared
    core files instead of live processes.
  - `test-core` runs core-file test suites on both debuggers.
  - `test-live` runs live test suites on both debuggers.

On macOS, with `lldb` installed globally, the output directory is
assumed to be `out/arm64.release`. Build and run the tests with:

```sh
autoninja -C out/arm64.release d8 v8_debug_helper_shared corruption_harness
make -C tools/debug_helper/plugins test-live-lldb
# To run core tests, first prepare cores, this can take a while
make -C tools/debug_helper/plugins prepare-cores
make -C tools/debug_helper/plugins test-core-lldb
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
OUT_DIR="$(pwd)/out/x64.release.test" make -C tools/debug_helper/plugins test-live-lldb
```

## Development Notes

Set `V8_DEBUG_HELPER_VERBOSE=1` to enable verbose logging in the plugins.

To see the raw output from the debugger without assertions, use the `run-*`
targets instead of `test-*`, for example:

```sh
make -C tools/debug_helper/plugins run-live-backtrace-lldb
```

## Directory Layout

- `shared_bridge.py`: shared `ctypes` bridge and `DebuggerBridge` class.
- `gdb_plugin.py`: GDB plugin entry point.
- `lldb_plugin.py`: LLDB plugin entry point.
- `test/`
  - `fixtures/`: test scripts and expected annotation fixtures.
  - `helpers/`: Python helpers for the tests
  - `test_gdb_live.py`: live-process GDB tests.
  - `test_lldb_live.py`: live-process LLDB tests.
  - `test_gdb_core.py`: core-file GDB tests.
  - `test_lldb_core.py`: core-file LLDB tests.

## Design

`libv8_debug_helper` is the dynamic library built by `v8_debug_helper_shared`,
which exposes C APIs whose definitions can be found in
`debug_helper.h`. The `DebuggerBridge` class in `shared_bridge.py`
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

The current `DebuggerBridge` API includes:

- `frame_suffix`: takes a frame pointer plus a memory-reading callback and
  returns the JS annotation suffix for that frame when enough V8 metadata
  can be recovered.
