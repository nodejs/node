---
name: v8-commands
description: "Key commands for building, debugging, and testing in V8. Use when needing command syntax for gm.py, d8 flags, or run-tests.py. Do not use for environment setup."
---

# V8 Key Commands

Use this skill to find the correct commands for common development tasks in V8. This unifies the commands listed in workspace configuration and ensures consistent usage.

## Building

V8 uses `gm.py` as a wrapper around GN and Ninja for building.

-   **Build (Debug):** `tools/dev/gm.py quiet x64.debug tests`
-   **Build (Optimized Debug):** `tools/dev/gm.py quiet x64.optdebug tests`
-   **Build (Release):** `tools/dev/gm.py quiet x64.release tests`

*Note: Always pass the `quiet` keyword unless told otherwise to avoid excessive output.*

## Debugging

Run `d8` with GDB or LLDB for native code debugging.

```bash
# Example of running d8 with gdb
gdb --args out/x64.debug/d8 --my-flag my-script.js
```

### Common Diagnostic Flags
- `--trace-opt`: Log optimized functions.
- `--trace-deopt`: Log when and why functions are deoptimized.
- `--trace-gc`: Log garbage collection events.
- `--allow-natives-syntax`: Enables calling of internal V8 functions (e.g., `%OptimizeFunctionOnNextCall(f)`) from JavaScript for testing purposes.
- `--gdbjit`: Allows seeing Maglev graphs or JavaScript code in GDB backtraces. To debug issues involving code motion disable gdbjit (as it disables code motion).

A comprehensive list of all flags can be found by running `out/x64.debug/d8 --help`.

## Testing

For detailed instructions on running tests and interpreting failures, see the dedicated test execution guidelines.

Key commands summary:
-   **Run tests:** `tools/run-tests.py --progress dots --outdir=out/x64.optdebug`
