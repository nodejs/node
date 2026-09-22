# WebAssembly Architecture in V8

This document describes the architecture of WebAssembly (Wasm) implementation in V8, focusing on its compilation pipeline.

## Overview

V8 provides a high-performance WebAssembly implementation. Similar to JavaScript, it uses a multi-tiered compilation strategy to balance startup time and peak performance.

## Compilation Tiers

Wasm execution in V8 typically goes through two main tiers (Liftoff and Turboshaft), but an interpreter is also available:

### 0. DrumBrake (Interpreter)

**DrumBrake** is V8's WebAssembly interpreter.
*   **Goal**: Support for systems where JIT compilation is not allowed (jitless mode) or for debugging.
*   **Mechanism**: It interprets Wasm bytecode directly.
*   **File**: `src/wasm/interpreter/wasm-interpreter.cc`

### 1. Liftoff (Baseline Compiler)

**Liftoff** is V8's baseline compiler for WebAssembly.
*   **Goal**: Extremely fast compilation to minimize startup latency.
*   **Mechanism**: It is a simple, one-pass compiler that generates machine code directly as it decodes the Wasm bytecode. It does not build a complex intermediate representation (IR) or perform heavy optimizations. Liftoff can compile WebAssembly code very fast, tens of megabytes per second.
*   **File**: `src/wasm/baseline/liftoff-compiler.cc`

### 2. Turboshaft (Optimizing Compiler)

For hot functions, V8 uses **Turboshaft** (which replaced TurboFan for Wasm) to generate highly optimized machine code.
*   **Goal**: Peak performance.
*   **Mechanism**: It builds a Turboshaft graph from the Wasm bytecode and applies a series of optimization phases (reducers) before generating machine code.
*   **File**: `src/wasm/turboshaft-graph-interface.cc` contains the logic to build a Turboshaft graph from Wasm.
*   **Entry Point**: `src/wasm/function-compiler.cc` calls `compiler::turboshaft::ExecuteTurboshaftWasmCompilation` to run the pipeline.

## Tiering and Execution Flow

V8 uses a dynamic tiering strategy to balance startup time and execution speed:
*   **Jitless Mode**: If jitless mode is enabled, execution starts and stays in the **DrumBrake** interpreter.
*   **Startup**: In standard mode, functions are initially compiled with **Liftoff** to get running as quickly as possible.
*   **Tier-Up**: V8 monitors execution frequency. When a function becomes "hot", it triggers a tier-up to **Turboshaft**. Note though that we don’t do on-stack-replacement for Wasm. This means that if Turboshaft code becomes available after the function was called, the function call will complete its execution with Liftoff code.
*   **Orchestration**: The `TriggerTierUp` function in `src/wasm/module-compiler.cc` manages queueing and executing these background optimization jobs.
*   **Fallback**: If Liftoff encounters an unsupported feature or fails, V8 falls back to the optimizing compiler immediately.

## Code Caching

If the WebAssembly module was compiled with `WebAssembly.compileStreaming` then the Turboshaft-generated machine code will also get cached. When the same WebAssembly module is fetched again from the same URL then the cached code can be used immediately without additional compilation. More information about code caching is available [in a separate blog post](/blog/wasm-code-caching).

Code caching gets triggered whenever the amount of generated Turboshaft code reaches a certain threshold. This means that for large WebAssembly modules the Turboshaft code gets cached incrementally, whereas for small WebAssembly modules the Turboshaft code may never get cached. Liftoff code does not get cached, as Liftoff compilation is nearly as fast as loading code from the cache.

## Debugging

As mentioned earlier, Turboshaft applies optimizations, many of which involve re-ordering code, eliminating variables or even skipping whole sections of code. This means that if you want to set a breakpoint at a specific instruction, it might not be clear where program execution should actually stop. In other words, Turboshaft code is not well suited for debugging. Therefore, when debugging is started by opening DevTools, all Turboshaft code is replaced by Liftoff code again ("tiered down"), as each WebAssembly instruction maps to exactly one section of machine code and all local and global variables are intact.

## Profiling

Within DevTools all code will get tiered up (recompiled with Turboshaft) again when the Performance tab is opened and the "Record" button in clicked. The "Record" button starts performance profiling. Profiling the Liftoff code would not be representative as it is only used while Turboshaft isn’t finished and can be significantly slower than Turboshaft’s output, which will be running for the vast majority of time.

## Flags for experimentation

For experimentation, V8 and Chrome can be configured to compile WebAssembly code only with Liftoff or only with Turboshaft. It is even possible to experiment with lazy compilation, where functions only get compiled when they get called for the first time. The following flags enable these experimental modes:

- **Liftoff only**:
    - In V8, set the `--liftoff --no-wasm-tier-up` flags.
    - In Chrome, disable WebAssembly tiering (`chrome://flags/#enable-webassembly-tiering`) and enable WebAssembly baseline compiler (`chrome://flags/#enable-webassembly-baseline`).

- **Turboshaft only**:
    - In V8, set the `--no-liftoff --no-wasm-tier-up` flags.
    - In Chrome, disable WebAssembly tiering (`chrome://flags/#enable-webassembly-tiering`) and disable WebAssembly baseline compiler (`chrome://flags/#enable-webassembly-baseline`).

- **Lazy compilation**:
    - Lazy compilation is a compilation mode where a function is only compiled when it is called for the first time. Similar to the production configuration the function is first compiled with Liftoff (blocking execution). After Liftoff compilation finishes, the function gets recompiled with Turboshaft in the background.
    - In V8, set the `--wasm-lazy-compilation` flag.
    - In Chrome, enable WebAssembly lazy compilation (`chrome://flags/#enable-webassembly-lazy-compilation`).

## Compile time

There are different ways to measure the compilation time of Liftoff and Turboshaft. In the production configuration of V8, the compilation time of Liftoff can be measured from JavaScript by measuring the time it takes for `new WebAssembly.Module()` to finish, or the time it takes `WebAssembly.compile()` to resolve the promise. To measure the compilation time of Turboshaft, one can do the same in a Turboshaft-only configuration.

The compilation can also be measured in more detail in `chrome://tracing/` by enabling the `v8.wasm` category. Liftoff compilation is then the time spent from starting the compilation until the `wasm.BaselineFinished` event, Turboshaft compilation ends at the `wasm.TopTierFinished` event. Compilation itself starts at the `wasm.StartStreamingCompilation` event for `WebAssembly.compileStreaming()`, at the `wasm.SyncCompile` event for `new WebAssembly.Module()`, and at the `wasm.AsyncCompile` event for `WebAssembly.compile()`, respectively. Liftoff compilation is indicated with `wasm.BaselineCompilation` events, Turboshaft compilation with `wasm.TopTierCompilation` events.

More detailed tracing data is available with the `v8.wasm.detailed` category, which, among other information, provides the compilation time of single functions.

## Key Components

*   **Module Decoder**: Parses the Wasm binary format (`src/wasm/module-decoder.h`).
*   **Function Body Decoder**: Decodes the bytecode of individual functions (`src/wasm/function-body-decoder.h`).
*   **Wasm Engine**: Manages the compilation tasks and code caching (`src/wasm/wasm-engine.h`).

## File Structure
*   `src/wasm/`: Core WebAssembly implementation.
*   `src/wasm/baseline/`: Liftoff baseline compiler.
*   `src/wasm/interpreter/`: DrumBrake interpreter.
*   `src/wasm/turboshaft-graph-interface.cc`: Wasm frontend for Turboshaft.
