---
name: v8-structure
description: "Maps the V8 folder structure and component locations. Use when locating specific subsystems or understanding repository architecture. Do not use for build command syntax."
---

# V8 Folder Structure

Use this skill to understand the layout of the V8 repository and locate specific components.

## Main Directories

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
