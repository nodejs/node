# V8 High-Level Overview

V8 is Google's open source high-performance JavaScript and WebAssembly engine, written in C++. It is used in Google Chrome and in Node.js, among others.

This document provides a high-level overview of V8's architecture and execution pipeline.

## Execution Pipeline

V8 executes JavaScript code using a multi-tier execution pipeline. The goal is to start executing code as quickly as possible and then optimize hot code for peak performance.

1.  **Parsing**: The source code is parsed into an Abstract Syntax Tree (AST).
2.  **Ignition (Interpreter)**: V8's interpreter, Ignition, generates bytecode from the AST and executes it. This allows for fast startup.
3.  **Sparkplug (Baseline Compiler)**: A fast non-optimizing compiler that compiles bytecode to machine code. It runs quickly and provides a decent speedup over interpretation.
4.  **Maglev (Mid-tier Compiler)**: An optimizing compiler that sits between Sparkplug and TurboFan. It generates better code than Sparkplug but compiles faster than TurboFan.
5.  **TurboFan / Turboshaft (Optimizing Compiler)**: V8's peak optimizing compiler tier. TurboFan is being transitioned to Turboshaft, which uses a new, more efficient linear intermediate representation (IR) based on a control-flow graph (CFG). It generates highly optimized machine code for hot functions.

## Key Components

### Parser (`src/parsing/`)
Responsible for scanning and parsing JavaScript source code into an Abstract Syntax Tree (AST).

### Interpreter (`src/interpreter/`)
Ignition is V8's register-based interpreter. It executes bytecode and collects profiling information used by optimizing compilers.

### Compilers
-   **Sparkplug (`src/baseline/`)**: Compiles bytecode directly to machine code without complex optimizations.
-   **Maglev (`src/maglev/`)**: Generates optimized code quickly, filling the gap between Sparkplug and TurboFan.
-   **TurboFan (`src/compiler/`)**: V8's traditional peak optimizing compiler based on a sea-of-nodes IR.
-   **Turboshaft (`src/compiler/turboshaft/`)**: The new optimizing compiler pipeline that replaces TurboFan's sea-of-nodes IR with a more efficient linear IR.

### Heap and Garbage Collector (`src/heap/`)
V8 manages memory in a garbage-collected heap. It uses a generational garbage collector with a young generation (semi-space) and an old generation. It also employs concurrent and parallel marking and sweeping techniques to minimize pause times.

### Objects and Maps (`src/objects/`)
V8 uses "Maps" (also known as hidden classes or shapes) to represent the structure of JavaScript objects. This allows for fast property access via Inline Caching (IC).

### WebAssembly (`src/wasm/`)
V8 provides a full-featured WebAssembly implementation, including a streaming compiler and integration with the JavaScript garbage collector.

### Sandbox (`src/sandbox/`)
A security feature that protects the integrity of the V8 heap by ensuring that memory corruptions cannot easily access memory outside the sandbox.

### Builtins (`src/builtins/`)
Implementations of JavaScript built-in objects and functions, often written in Torque or CodeStubAssembler for performance.

### Snapshots (`src/snapshot/`)
Used to speed up startup by serializing the heap state and deserializing it when new isolates are created.

### Public API (`include/` and `src/api/`)
V8 can be embedded in other C++ applications. The public API is defined in `include/` and implemented in `src/api/`.

## See Also
-   [v8.dev/docs](https://v8.dev/docs) for official documentation.
