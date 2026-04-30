---
name: torque
description: "Provides expert guidance for navigating, implementing, and verifying V8 Torque (.tq) builtins. Use when modifying or debugging Torque files. Do not use for C++ macro builtins."
---

# V8 Torque

Torque is an Ahead-of-Time (AOT) generator that transforms `.tq` DSL into highly-optimized C++ CodeStubAssembler (CSA) code, which is then compiled into the `mksnapshot` binary.

## Execution Pipeline

To debug failures, you must understand its multi-stage build:

1.  **Generation**: The Torque compiler reads `.tq` files and generates C++ CodeStubAssembler files (`.cc`, `.h`, `.inc`) in `out/<config>/gen/torque-generated/`.
2.  **Compilation**: `mksnapshot` (or the `d8` build) compiles the generated C++ files.
3.  **Snapshotting**: `mksnapshot` executes the generated C++ code (via TurboFan or Maglev) to emit highly-optimized raw machine code.
4.  **Linking**: The machine code is serialized into a snapshot and linked into V8, meaning Torque builtins run with zero translation overhead at runtime.

## Implementation Patterns
Common Torque syntax, keywords, and linkage definitions:

```cpp
// 1. Type Casting & Checks
const array = Cast<JSArray>(object) otherwise GotoLabel;
if (Is<JSArray>(object)) { ... }

// 2. Control Flow
try {
  const smi = Cast<Smi>(input) otherwise IsNotSmi;
} label IsNotSmi deferred {
  return runtime::DoSomething(context, input);
}

// 3. Signatures & Keywords
// 'macro': Inlined functions for reusable logic.
transitioning macro Name(implicit context: Context)(arg: JSAny): JSAny

// 'builtin': Non-inlined functions, callable from other builtins or JavaScript.
transitioning builtin Name(implicit context: Context)(arg: JSAny): JSAny

// 'javascript': Marks a builtin as directly callable from JavaScript, with JS linkage.
transitioning javascript builtin Name(js-implicit context: NativeContext, receiver: JSAny)(arg: JSAny): JSAny

// 'transitioning': Indicates a function can cause an object's map to change (e.g. adding properties).
// 'extern': Used to call C++ defined CSA functions from Torque.
extern transitioning macro NameInCpp(Context, JSAny): JSAny;
```



## Mandatory verification workflow

The task is **incomplete** until you successfully execute this sequence:

### 1. Build
Run the `gm.py` wrapper to trigger the Torque generator and C++ compilation.

```bash
tools/dev/gm.py quiet {arch}.{type}
```

(e.g., `x64.optdebug` or `arm64.release`). Use `optdebug` for logic/debugging, `release` for performance/benchmarking.

### 2. Verify with Tests
Run the relevant test suite (usually `mjsunit` for JavaScript-exposed builtins) to ensure correctness. Match the `{arch}.{type}` to your build.

```bash
tools/run-tests.py --progress dots --outdir=out/{arch}.{type} mjsunit/<test_name>
```

### 3. Debug (If Necessary)
If the build fails during "Generation", inspect the `.tq` syntax. If it fails during "Compilation", check the generated C++ in `out/<config>/gen/torque-generated/`.
