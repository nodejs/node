---
name: v8-testing
description: Executes and interprets V8 tests. Use when running unittests or mjsunit, or when debugging test failures. Do not use for building.
---

# V8 Testing

Use this skill to understand how to run tests in V8 and how to handle failures.
This specializes the general commands for testing.

## Primary Script

The primary script for running tests is `tools/run-tests.py`. You specify the
build output directory and the tests you want to run.

## Key Test Suites

- **unittests**: C++ unit tests (`v8_unittests`, GoogleTest) for V8's internal
  components. `tools/run-tests.py` executes each test (`TEST`, `TEST_F`,
  `TEST_P`) in its own isolated process (via `--fork-server` on POSIX or a fresh
  subprocess with `--gtest_filter=<Suite.Test>`).
- **mjsunit**: JavaScript-based tests for JavaScript language features and
  builtins. Each `.js` file runs in a fresh `d8` process.
- **inspector**: JavaScript-based CDP tests under `test/inspector/` executed
  against `inspector-test` (using `test/inspector/protocol-test.js`). Unlike
  `d8 --enable-inspector`, `inspector-test` runs the real inspector message loop
  and `V8Console`.

## Running Tests

```bash
# Run all standard tests for the x64.optdebug build
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug

# Run a specific test suite (e.g., unittests)
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug unittests

# Run unittests matching a glob via the test runner (preserves per-test process isolation)
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug 'unittests/Torque*'

# Direct binary execution (for debugging a single test, e.g. under gdb)
# Always pass a single test to preserve process isolation; use tools/run-tests.py
# when running multiple tests or wildcards.
out/x64.optdebug/v8_unittests --gtest_filter=TorqueTest.Scanner

# Manual execution with d8
# Note: mjsunit tests always require test/mjsunit/mjsunit.js.
# tools/run-tests.py prepends this automatically for all tests in mjsunit,
# so you don't need to load it explicitly in the test file.
# For manual d8 execution, you must include it first.
# You can also run tools/run-tests.py with -v to see the full command line used by the runner.
out/x64.optdebug/d8 test/mjsunit/mjsunit.js test/mjsunit/my-test.js

# Run inspector tests via run-tests.py or directly with inspector-test
tools/run-tests.py --progress dots --outdir=out/x64.optdebug 'inspector/*'
out/x64.optdebug/inspector-test test/inspector/protocol-test.js test/inspector/runtime/evaluate.js
```

*Note: Always pass `--progress dots` so that there is minimal progress
reporting, to avoid cluttering the output.*

## Writing Inspector & JS Tests

- **Wasm `Debugger.scriptParsed` ordering (`test/inspector/`):** When
  `Debugger.enable` is active, compiling or evaluating a script that
  instantiates Wasm emits two `Debugger.scriptParsed` events (first the JS
  wrapper script, second `wasm://...`). Await
  `Protocol.Debugger.onceScriptParsed(2)` or filter `onScriptParsed` by
  `msg.params.url.startsWith('wasm://')` rather than calling
  `onceScriptParsed()` once.
- **Background tasks & uncaught exceptions (`test/inspector/`):**
  `WebAssembly.compile(...)` resolves on background compilation threads that
  `Protocol.Runtime.evaluate({awaitPromise: true})` does not pump; start work
  without `awaitPromise` and call `await InspectorTest.waitForPendingTasks()`.
  Because `Protocol.Runtime.evaluate` runs inside an outer C++ `TryCatch` that
  masks uncaught-exception prediction in `try ... finally`, schedule top-level
  code via `setTimeout(...)` followed by
  `await InspectorTest.waitForPendingTasks()` when testing uncaught exceptions.
- **Module registration order & large fixtures (`test/inspector/`):**
  `contextGroup.addModule(...)` resolves imports only against modules already
  registered in `contextGroup`, so register dependency modules before importing
  modules. To test length or serialization limits cheaply in Debug builds, use
  sparse dictionary-mode arrays (`const a = []; a[6e6 - 1] = 0;`) or
  `'a'.repeat(%StringMaxLength() - n)` cons-strings (`--allow-natives-syntax`).
- **Sandbox/memory-corruption flags in `inspector.status`:** Gate tests using
  `--expose-memory-corruption-api` or `--sandbox-testing` on
  `not memory_corruption_api` in `test/inspector/inspector.status`. Never add
  `--no-verify-heap` to `// Flags:` in `test/inspector/` (Debug builds pass
  `--verify-heap`, causing `inspector-test` to abort on contradictory flags).
- **`git cl format` on JS tests (`% Natives` & `// Flags:`):**
  `git cl format --js` inserts a space after `%` in `--allow-natives-syntax`
  calls (`% Foo(...)`), which fails `PRESUBMIT.py`
  (`unexpected spaces in a runtime call`); remove the space before uploading. In
  addition, `tools/testrunner/objects/testcase.py` matches `//\s+Flags:(.*)`,
  and `clang-format --js` wraps `// Flags:` lines over 80 columns onto
  `// --flag` lines that the runner ignores; keep each `// Flags:` line under 80
  columns and split long flag lists across multiple `// Flags:` lines.

## Testing Strategy

To maximize efficiency, use different build configurations based on the task:

- **`x64.debug`**: Use this for deep debugging of a single test case (e.g., with
  GDB). It provides full debug information and assertions, but is extremely slow
  and **must be avoided** for running `mjsunit` tests in bulk.
- **`x64.optdebug`**: Use this for running the entire test suite. It strikes a
  balance between performance and debuggability, allowing for faster full-suite
  checks while still preserving assertions.
- **`x64.release`**: Use this for quick checks to verify that a fix works in a
  release build or to run benchmarks. It is the fastest but provides the least
  debug information.

## Interpreting Failures

If there are any failing tests, they will be reported along their stderr and a
command to reproduce them e.g.

```
=== mjsunit/maglev/regress-429656023 ===
--- stderr ---
#
# Fatal error in ../../src/heap/local-factory.h, line 41
# unreachable code
#
...stack trace...
Received signal 6
Command: out/x64.optdebug/d8 --test test/mjsunit/mjsunit.js test/mjsunit/maglev/regress-429656023.js --random-seed=-190258694 --nohard-abort --verify-heap --allow-natives-syntax
```

You can retry the test either by running the test name with
`tools/run-tests.py`, e.g.
`tools/run-tests.py --progress dots --outdir=out/x64.optdebug mjsunit/maglev/regress-429656023`,
or by running the command directly. When running the command directly, you can
add additional flags to help debug the issue, and you can try running a
different build (e.g. running a debug build if a release build fails).
