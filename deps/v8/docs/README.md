# V8 Internals Documentation

This directory contains deep-dive documentation into V8 internals, generated from source code analysis. For general documentation, see [v8.dev/docs](https://v8.dev/docs).

## Table of Contents

### General
*   [High-Level Overview](overview.md): High-level overview of V8's architecture and execution pipeline.
*   [JavaScript Overview](javascript-overview.md): High-level overview of JavaScript concepts for V8 developers.
*   [Security Bug Triaging](security/triaging.md): How V8 triages security bugs.

### Heap and Memory
*   [Heap Overview](heap/heap-overview.md): General overview of V8's paged heap and object layout.
*   [Garbage Collection](heap/garbage-collection.md): Overview of V8's generational GC.
*   [Marking and Sweeping](heap/marking-and-sweeping.md): Deep dive into Major GC phases.
*   [Roots](heap/roots.md): How V8 manages root objects and static roots.
*   [Handles](heap/handles.md): V8's mechanism for safe heap object references (Indirect vs Direct).
*   [Heap Compaction](heap/compaction.md): How V8 resolves memory fragmentation in the Old Generation.
*   [Pointer Compression](heap/pointer-compression.md): Technique to reduce memory footprint on 64-bit architectures.
*   [Pointer Tagging](heap/pointer-tagging.md): How V8 encodes type information in pointers (Smis vs HeapObjects).
*   [Zone Allocator](heap/zone-allocator.md): Region-based memory manager for fast allocation and batch deallocation.

### Object Model
*   [Objects and Maps](heap/objects-and-maps.md): How JavaScript objects and Maps (hidden classes) are represented in V8's heap.
*   [Descriptor and Transition Arrays](objects/descriptor-and-transition-arrays.md): How V8 tracks property additions and shares descriptor arrays between maps.
*   [Field Representations and Elements Kinds](objects/fields-and-elements.md): How V8 specializes storage for properties and elements based on their types.
*   [Lookup Iterator](objects/lookup-iterator.md): V8's central mechanism for resolving property accesses.
*   [Strings](objects/strings.md): Overview of V8's complex string representation and transitions.

### Sandbox
*   [Architecture](sandbox/architecture.md): V8's in-process sandbox to protect memory outside of it.

### Torque
*   [Architecture](torque/architecture.md): V8's domain-specific language for writing builtins.
*   [User Manual](torque/user-manual.md): The V8 Torque language manual.

### Builtins
*   [Architecture](builtins/architecture.md): How V8 implements standard library and runtime helpers (Torque, CSA, C++, ASM).
*   [Torque Tutorial](builtins/torque-tutorial.md): Introduction to writing Torque builtins.

### Code Generation
*   [Assembler and MacroAssembler](codegen/assembler-architecture.md): Low-level code generation abstractions.
*   [CodeStubAssembler](codegen/code-stub-assembler.md): Portable macro-assembler for writing builtins and bytecode handlers.
*   [CodeStubAssembler Tutorial](codegen/csa-tutorial.md): Introduction to writing CodeStubAssembler builtins.

### RegExp
*   [Architecture](regexp/architecture.md): Irregexp, V8's regular expression engine.

### WebAssembly
*   [Architecture](wasm/architecture.md): V8's WebAssembly implementation, including Liftoff and Turboshaft tiers.

### Snapshots
*   [Architecture](snapshot/architecture.md): V8's snapshot and serializer system for fast startup.

### Execution - Runtime
*   [Ignition (Interpreter)](interpreter/interpreter-ignition.md): V8's register-based interpreter.
*   [Hidden Classes and Inline Caches](runtime/hidden-classes-and-ics.md): How V8 optimizes property access using Maps and ICs.
*   [Maps (Hidden Classes) Tutorial](runtime/hidden-classes-tutorial.md): Tutorial on how V8 builds hidden classes.
*   [Scopes and ScopeInfos](runtime/scopes-and-scope-infos.md): The relationship between compile-time Scopes and runtime ScopeInfos.
*   [Function Architecture](runtime/function-architecture.md): How V8 splits function data into context-independent (SFI) and context-dependent (JSFunction) parts, and how they are chained.
*   [Tiering and Interrupt Budget](runtime/tiering.md): How V8 decides to optimize functions.
*   [Exception Handling](runtime/exception-handling.md): How V8 implements exceptions using side-tables across all tiers.
*   [Deoptimization](runtime/deoptimization.md): How V8 transitions from optimized code back to the interpreter.
*   [Stack Walking](runtime/stack-walking.md): How V8 traverses and inspects call stack frames.

### Compiler

#### Sparkplug
*   [Sparkplug (Baseline Compiler)](compiler/sparkplug/compiler-sparkplug.md): Fast, non-optimizing compiler that mirrors the interpreter.

#### Maglev
*   [Maglev (Mid-Tier Compiler)](compiler/maglev/compiler-maglev.md): Graph-based optimizing compiler.
*   [Maglev Register Allocator](compiler/maglev/compiler-maglev-regalloc.md): Linear scan and node constraints.

#### TurboFan
*   [TurboFan (Optimizing Compiler)](compiler/turbofan/compiler-turbofan.md): Top-tier optimizing compiler using Sea-of-Nodes and Turboshaft IRs.

#### Turboshaft
*   [Turboshaft and Turbolev](compiler/turboshaft/compiler-turboshaft.md): Next-generation IR and pipeline, including the Maglev frontend.
*   [Turboshaft IR Details](compiler/turboshaft/compiler-turboshaft-ir.md): Operations and the side-effect system.
*   [Turboshaft Reducers](compiler/turboshaft/compiler-turboshaft-reducers.md): Composable optimization passes.
*   [Turboshaft Copying Approach](compiler/turboshaft/compiler-turboshaft-copying-approach.md): Semi-spaces for IR and cheap copies.

#### Common
*   [Why V8 Embraced CFG](compiler/why-cfg.md): The transition from TurboFan's SoN to Turboshaft's CFG and its benefits.
*   [Concurrency and Background Compilation](compiler/concurrency-and-background-compilation.md): How V8 uses background threads safely.

### Parsing
*   [Parser and AST](parsing/parser-and-ast.md): How V8 parses JavaScript into an Abstract Syntax Tree.
*   [Lazy Parsing and Preparser](parsing/lazy-parsing-and-preparser.md): How V8 delays parsing and handles closures.
