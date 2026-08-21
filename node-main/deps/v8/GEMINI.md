# Gemini Workspace for V8

This is the workspace configuration for V8 when using Gemini.

Documentation can be found at https://v8.dev/docs.

## Key Commands

-   **Build (Debug):** `tools/dev/gm.py quiet x64.debug`
-   **Build (Optimized Debug):** `tools/dev/gm.py quiet x64.optdebug`
-   **Build (Release):** `tools/dev/gm.py quiet x64.release`
-   **Run All Tests:** `tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug`
-   **Run C++ Tests:** `tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug cctest unittests`
-   **Run JavaScript Tests:** `tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug mjsunit`
-   **Format Code:** `git cl format`

Some hints:
- You are an expert C++ developer.
- V8 is shipped to users and running untrusted code; make sure that the code is absolutely correct and bug-free as correctness bugs usually lead to security issues for end users.
- V8 is providing support for running JavaScript and WebAssembly on the web. As such, it is critical to aim for best possible performance when optimizing V8.

## Folder structure

- `src/`: The main source folder providing the implementation of the virtual machine. Key subdirectories include:
    - `src/api/`: Implements the V8 public C++ API, as declared in `include/`.
    - `src/asmjs/`: Contains V8's Asm.js pipeline, which compiles the Asm.js subset of JavaScript into WebAssembly.
    - `src/ast/`: Defines the Abstract Syntax Tree (AST) used to represent parsed JavaScript, including nodes, scopes, and variables.
    - `src/base/`: Provides fundamental, low-level utilities, data structures, and a platform abstraction layer for the entire V8 project.
    - `src/baseline/`: Implements the Sparkplug baseline compiler, which generates machine code directly from bytecode for a fast performance boost.
    - `src/bigint/`: The implementation of BigInt operations.
    - `src/builtins/`: Implementation of JavaScript built-in functions (e.g., `Array.prototype.map`).
    - `src/codegen/`: Code generation, including direct machine code generation via macro assemblers, higher level codegen via CodeStubAssembler, definitions of machine code metadata like safepoint tables and source position tables, and `compiler.cc` which defines entry points into the compilers. This contains subdirectories for architecture specific implementations, which should be kept in sync with each other as much as possible.
    - `src/common/`: Common definitions and utilities.
    - `src/compiler/`: The TurboFan optimizing compiler, including the Turboshaft CFG compiler.
    - `src/d8/`: The `d8` shell implementation, for running V8 in a CLI.
    - `src/debug/`: The debugger and debug protocol implementation.
    - `src/deoptimizer/`: The deoptimizer implementation, which translates optimized frames into unoptimized ones.
    - `src/execution/`: The definitions of the execution environment, including the Isolate, frame definitions, microtasks, stack guards, tiering, and on-stack argument handling.
    - `src/handles/`: The handle implementation for GC-safe object references.
    - `src/heap/`: The garbage collector and memory management code.
    - `src/ic/`: The Inline Caching implementation.
    - `src/init/`: The V8 initialization code.
    - `src/inspector/`: The inspector protocol implementation.
    - `src/interpreter/`: The Ignition bytecode compiler and interpreter.
    - `src/json/`: The JSON parser and serializer.
    - `src/libplatform/`: The platform abstraction layer, for task runners and worker threads.
    - `src/logging/`: The logging implementation.
    - `src/maglev/`: The Maglev mid-tier optimizing compiler.
    - `src/numbers/`: Implementations of various numeric operations.
    - `src/objects/`: The representation and behaviour of V8 internal and JavaScript objects.
    - `src/parsing/`: The parser and scanner implementation.
    - `src/profiler/`: The in-process profiler implementations, for heap snapshots, allocation tracking, and a sampling CPU profiler.
    - `src/regexp/`: The regular expression implementation. This contains subdirectories for architecture specific implementations, which should be kept in sync with each other as much as possible.
    - `src/runtime/`: C++ functions that can be called from JavaScript at runtime.
    - `src/sandbox/`: The implementation of the sandbox, which is a security feature that attempts to limit V8 memory operations to be within a single guarded virtual memory allocation, such that corruptions of objects within the sandbox cannot lead to corruption of objects outside of it.
    - `src/snapshot/`: The snapshot implementation, for both the startup snapshot (read-only, startup heap, and startup context), as well as the code-serializer, which generates code caches for caching of user script code.
    - `src/strings/`: Implementations of string helpers, such as predicates for characters, unicode processing, hashing and string building.
    - `src/torque/`: The Torque language implementation.
    - `src/tracing/`: The tracing implementation.
    - `src/trap-handler/`: Implementations of trap handlers.
    - `src/wasm/`: The WebAssembly implementation.
    - `src/zone/`: The implementation of a simple bump-pointer region-based zone allocator.
- `test/`: Folder containing most of the tests and testing code.
- `include/`: Folder containing all of V8's publicAPI that is used when V8 is embedded in other projects such as e.g. the Blink rendering engine.
- `out/`: Folder containing the results of a build. Usually organized in sub folders for the respective configurations.

## Building

The full documentation for building using GN can be found at https://v8.dev/docs/build-gn.

Once the initial dependencies are installed, V8 can be built using `gm.py`, which is a wrapper around GN and Ninja.

```bash
# List all available build configurations and targets
tools/dev/gm.py

# Build the d8 shell for x64 in release mode
tools/dev/gm.py quiet x64.release

# Build d8 for x64 in debug mode
tools/dev/gm.py quiet x64.debug
```

- **release:** Optimized for performance, with debug information stripped. Use for benchmarking.
- **debug:** Contains full debug information and enables assertions. Slower, but essential for debugging.
- **optdebug:** A compromise with optimizations enabled and debug information included. Good for general development.

Make sure to pass the `quiet` keyword unless told to otherwise, so that you don't waste tokens on compilation progress. Errors will still be reported.

## Debugging

For debugging, it is recommended to use a `debug` or `optdebug` build. You can run `d8` with GDB or LLDB for native code debugging.

```bash
# Example of running d8 with gdb
gdb --args out/x64.debug/d8 --my-flag my-script.js
```

V8 also provides a rich set of flags for diagnostics. Some of the most common ones are:
- `--trace-opt`: Log optimized functions.
- `--trace-deopt`: Log when and why functions are deoptimized.
- `--trace-gc`: Log garbage collection events.
- `--allow-natives-syntax`: Enables calling of internal V8 functions (e.g. `%OptimizeFunctionOnNextCall(f)`) from JavaScript for testing purposes.

A comprehensive list of all flags can be found by running `out/x64.debug/d8 --help`. Most V8 flags are in `flag-definitions.h`; flags specific to the `d8` shell are located in `src/d8/d8.cc` within the `Shell::SetOptions` function.

When debugging issues in Torque code, it is often useful to inspect the generated C++ files in `out/<build-config>/gen/torque-generated/`. This allows you to see the low-level CodeStubAssembler code that is actually being executed.

## Testing

The primary script for running tests is `tools/run-tests.py`. You specify the build output directory and the tests you want to run. Key test suites include:
- **unittests:** C++ unit tests for V8's internal components.
- **cctest:** Another, older format for C++ unit tests (deprecated, in the process of being moved to unittests).
- **mjsunit:** JavaScript-based tests for JavaScript language features and builtins.

```bash
# Run all standard tests for the x64.optdebug build
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug

# Run a specific test suite (e.g., cctest)
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug cctest

# Run a specific test file
tools/run-tests.py --progress dots --exit-after-n-failures=5 --outdir=out/x64.optdebug cctest/test-heap
```

It's important to pass `--progress dots` so that there is minimal progress reporting, to avoid cluttering the output.

If there are any failing tests, they will be reported along their stderr and a command to reproduce them e.g.

```
=== mjsunit/maglev/regress-429656023 ===
--- stderr ---
#
# Fatal error in ../../src/heap/local-factory.h, line 41
# unreachable code
#
#
#
...stack trace...
Received signal 6
Command: out/x64.optdebug/d8 --test test/mjsunit/mjsunit.js test/mjsunit/maglev/regress-429656023.js --random-seed=-190258694 --nohard-abort --verify-heap --testing-d8-test-runner --allow-natives-syntax
```

You can retry the test either by running the test name with `tools/run-tests.py`, e.g. `tools/run-tests.py --progress dots --outdir=out/x64.optdebug mjsunit/maglev/regress-429656023`, or by running the command directly. When running the command directly, you can add additional flags to help debug the issue, and you can try running a different build (e.g. running a debug build if a release build fails).

The full testing documentation is at https://v8.dev/docs/test.

## Coding and Committing

- Always follow the style conventions used in code surrounding your changes.
- Otherwise, follow [Chromium's C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md).
- Use `git cl format` to automatically format your changes.

### Commit Messages
Commit messages should follow the convention described at https:/v8.dev/docs/contribute#commit-messages. A typical format is:

```
[component]: Short description of the change

Longer description explaining the "why" of the change, not just
the "what". Wrap lines at 72 characters.

Bug: 123456
```

- The `component` is the area of the codebase (e.g., `compiler`, `runtime`, `api`).
- The `Bug:` line is important for linking to issues in the tracker at https://crbug.com/

## Working with Torque

Torque is a V8-specific language used to write V8 builtins and some V8 object definitions. It provides a higher-level syntax that compiles down to CSA code.

### Key Concepts

-   **Purpose:** Simplify the creation of V8 builtins and object definitions by providing a more abstract language than writing CodeStubAssembler code directly.
-   **File Extension:** `.tq`
-   **Location:** Torque files are primarily located in `src/builtins` and `src/objects`.
-   **Compilation:** Torque files are compiled by the `torque` compiler, which generates C++ and Code Stub Assembler (CSA) files. These generated files are placed in the `out/<build-config>/gen/torque-generated/` directory and then compiled as part of the normal V8 build process.
    -   **C++ files** `*.tq` files will generate filenames like `*-tq.inc`, `*-tq.cc`, and `*-tq-inl.inc`. Additionally, there are top-level files:
        -   `class-forward-declarations.h`: Forward declarations for all Torque-defined classes.
        -   `builtin-definitions.h`: A list of all defined builtins.
        -   `csa-types.h`: Type definitions for the Code Stub Assembler.
        -   `factory.cc` and `factory.inc`: Factory functions for creating instances of Torque-defined classes.
        -   `class-verifiers.h` and `.cc`: Heap object verification functions (for debug builds).
        -   `exported-macros-assembler.h` and `.cc`: C++ declarations and definitions for exported Torque macros.
        -   `objects-body-descriptors-inl.inc`: Inline definitions for object body descriptors, which define the memory layout of objects.
        -   `objects-printer.cc`: Object printer functions for debugging.
        -   `instance-types.h`: The `InstanceType` enum, used to identify object types at runtime.
        -   `interface-descriptors.inc`: Definitions for call interface descriptors, which manage function call conventions.
    -   **CSA files** These have filenames like `*-csa.cc` and `*-csa.h`. They contain the C++ code that uses the `CodeStubAssembler` API to generate the low-level implementation of builtins.

### Syntax and Features

-   **Typescript-like Syntax:** Torque's syntax is similar to Typescript with support for functions (macros and builtins), variables, types, and control flow.
-   **Macros and Builtins:**
    -   `macro`: Inlined functions for reusable logic.
    -   `builtin`: Non-inlined functions, callable from other builtins or JavaScript.
-   **`extern` Keyword:** Used to call C++ defined CSA functions from Torque. This is how Torque code interfaces with the rest of the V8 codebase.
-   **`transitioning` and `javascript` Keywords:**
    -   `transitioning`: Indicates a function can cause an object's map to change (e.g., when a property is added to a JSObject).
    -   `javascript`: Marks a builtin as being directly callable from JavaScript, with Javascript linkage.
-   **Type System:** Torque has a strong type system that mirrors the V8 object hierarchy. This allows for compile-time type checking and safer code.
-   **Labels and `goto`:** Torque uses a `labels` and `goto` system for control flow, which is particularly useful for handling exceptional cases and optimizing performance.

### Workflow for Modifying Torque Files

1.  **Identify the relevant `.tq` file:** Builtins are in `src/builtins`, and object definitions are in `src/objects`.
2.  **Modify the Torque code:** Make the necessary changes to the `.tq` file, following the existing syntax and conventions.
3.  **Rebuild V8:** Run the appropriate `gm.py` command (e.g., `tools/dev/gm.py x64.release`) to recompile V8. This will automatically run the Torque compiler and build the generated C++ files.
4.  **Test your changes:** Run the relevant tests to ensure that your changes are correct and have not introduced any regressions.

### Example

A simple Torque macro to add two SMIs might look like this:

```torque
macro AddTwoSmis(a: Smi, b: Smi): Smi {
  return a + b;
}
```

A more complex example showing a JavaScript-callable builtin:

```torque
transitioning javascript builtin MyAwesomeBuiltin(
    js-implicit context: NativeContext)(x: JSAny): Number {
  // ... implementation ...
}
```

## Common Pitfalls & Best Practices

-   **Always format before committing:** Run `git cl format` before creating a commit to ensure your code adheres to the style guide.
-   **Do not edit generated files:** Files in `out/` are generated by the build process. Edits should be made to the source files (e.g., `.tq` files for Torque, `.pdl` for protocol definitions).
-   **Match test configuration to build:** Ensure you are running tests against the correct build type (e.g., run mjsunit from out/x64.debug if you built x64.debug).
-   **Check surrounding code for conventions:** Before adding new code, always study the existing patterns, naming conventions, and architectural choices in the file and directory you are working in.
-   **Avoid changing unrelated code:** Keep diffs small by only changing the code you intended to change. Nearby code should not be cleaned up while making a change -- if you think there is a good cleanup, suggest it to me for a separate patch.
-   **Keep related functions together:** When adding a new function, try to insert it near related functions, to keep similar behaviour close.
-   **Don't guess header names:** If you don't know where the definition of a class or function is, don't try to guess the header name, but search for it instead.
-   **Be careful with forward declarations:** Many types are forward declared; if you want to use them, you'll need to find the definition.
-   **Be careful with inline function definitions:** Many functions are declared as `inline` in the `.h` file, and defined in a `-inl.h` file. If you get compile errors about a missing definition, you are likely missing an `#include` for a `-inl.h` file. You can only include `-inl.h` files from other `-inl.h` files and `.cc` files.
