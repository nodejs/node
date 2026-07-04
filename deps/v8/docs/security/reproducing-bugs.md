# Reproducing Security Bugs

V8 offers many different configurations and modes.
Not all bugs filed are actually security bugs.
This document describes some of the configurations that can help with identifying whether a bug is a vulnerability or just a regular correctness bug.

## Regular security bugs

Reproductions (POCs) should work on `d8`.
To help identify whether a bug is a valid vulnerability or a regular correctness bug, various flags are used to filter out configurations that are not relevant for security testing.

The recommended way to verify if a bug is a security vulnerability is to run the reproduction with `--run-as-security-poc` in addition to any provided flags.
If the crash still occurs with this flag enabled, it is likely a valid security vulnerability (**Type=Vulnerability**).

### Fine-grained control over configurations

`--run-as-security-poc` implies the following flags, which can also be manually set to narrow down the problem space:

- `--disallow-unsafe-flags`: By default, `d8` runs with a variety of flags that are considered unsafe in the context of security testing.
This flag disables them.
**Setting this flag is the bare minimum requirement for a bug to be considered a security bug.**
- `--disallow-developer-only-features`: Prevents the usage of features that are only available to developers.
- `--no-experimental`: Prevents the usage of experimental features that are not yet ready for shipping.
- `--fuzzing`: Enables internal fuzzing support. It is often used to expose edge cases.

If a POC stops reproducing when any of the above flags are set, it is likely not a security bug (**Type=Bug**, **Security_Impact-None**).

## Sandbox security bugs

The steps are generally similar to regular security bugs.
The difference is the bugs are expected to reproduce with `--run-as-sandbox-security-poc` in addition to any provided flags.
A successful reproduction must result in the crash filter reporting a sandbox violation (e.g. "V8 sandbox violation detected") when run in the [sandbox testing environment](../../src/sandbox/README.md#testing).
Crashes reported as "harmless" by the filter do not qualify as sandbox escapes.
If the crash still occurs with this flag enabled, and the test filter does not consider it harmless, it is likely a valid security vulnerability (**Type=Vulnerability**).

## Useful helper functions and building blocks

Internal helper functions are exposed via JavaScript and are not generally available in production environments.
They can be very useful in constructing POCs.
These flags and helpers always need to be combined with the aforementioned sanitization flags to rule out misconfigurations.

Running with `--allow-natives-syntax --expose-gc` exposes:
- **Optimization**: `%PrepareFunctionForOptimization(func)`, `%OptimizeFunctionOnNextCall(func)`, `%OptimizeMaglevOnNextCall(func)`, `%OptimizeOsr(optional_frame_depth)`, `%NeverOptimizeFunction(func)`, `%DeoptimizeFunction(func)`, `%DeoptimizeNow()`
- **Tiering**: `%ActiveTierIsIgnition(func)`, `%ActiveTierIsSparkplug(func)`, `%ActiveTierIsMaglev(func)`, `%ActiveTierIsTurbofan(func)`, `%GetOptimizationStatus(func)`
- **WebAssembly**: `%WasmTierUpFunction(func)`
- **Debugging**: `%DebugPrint(value)`, `%DisassembleFunction(func)` (requires `is_debug`), `%HeapObjectVerify(obj)` (requires `is_debug` or `v8_enable_verify_heap`), `%SystemBreak()`
- **Garbage Collection**: `gc()`, `%SimulateNewspaceFull()`
- **Synchronization**: `%BlockAt(synchronization_point, timeout)`, `%WaitUntilBlocked(synchronization_point, timeout)`, `%Resume(synchronization_point)`

For WebAssembly, the `WasmModuleBuilder` (`test/mjsunit/wasm/wasm-module-builder.js`) should be used to construct POCs whenever possible.
The easiest way is to just invoke the script from JavaScript in `d8` using `d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js')`.
Alternatively, it can be passed to `d8` before the POC script (e.g. `d8 test/mjsunit/wasm/wasm-module-builder.js poc.js`).
