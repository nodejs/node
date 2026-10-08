# Turboshaft IR Details

This document dives into the details of Turboshaft's Intermediate Representation (IR), focusing on operation definitions and the side-effect system.

## Overview

Turboshaft is V8's next-generation optimizing compiler. Unlike its predecessor, TurboFan, which used a "Sea-of-Nodes" IR where control and data flow were intermingled, Turboshaft uses a more traditional **Control Flow Graph (CFG)** representation.

In Turboshaft:
*   The graph consists of **Blocks**.
*   Each block contains a linear sequence of **Operations**.
*   Control flow is explicit at the end of blocks (e.g., `Goto`, `Branch`, `Switch`).

## Operation Definition

Operations are defined in `src/compiler/turboshaft/operations.h`. Each operation is represented by a C++ `struct` (e.g., `WordBinopOp`, `LoadOp`) and has a corresponding `Opcode`.

### Operation Categories
Turboshaft categorizes operations into several lists:
*   **Machine Operations**: Low-level operations that map closely to machine instructions (e.g., `WordBinop`, `FloatBinop`, `Load`, `Store`).
*   **Simplified Operations**: Higher-level operations that are lowered to machine operations (e.g., `CheckedClosure`, `CheckMaps`).
*   **JS Operations**: Operations representing JavaScript semantics (e.g., `GenericBinop`).
*   **Wasm Operations**: WebAssembly-specific operations.

### Defining a New Operation
To define a new operation `Foo`:
1.  Add `V(Foo)` to the appropriate operation list macro (e.g., `TURBOSHAFT_MACHINE_OPERATION_LIST`).
2.  Define a `struct FooOp` deriving from `OperationT<FooOp>` or `FixedArityOperationT<N, FooOp>` (if it has fixed `N` inputs).
3.  The struct should contain:
    *   Public fields for options.
    *   A getter `options()` returning a tuple of these options (used for printing and hashing), or custom `PrintOptions` and `hash_value`.
    *   Getters for named inputs.
    *   A constructor taking inputs first and then options.
    *   `outputs_rep()` and `inputs_rep()` methods defining representations.
    *   `OpEffects` as a static `effects` member or non-static `Effects()` method.
4.  If the operation is not lowered before instruction selection, handle it in `instruction-selector.cc`.

## The Side-Effect System

One of Turboshaft's most innovative features is its fine-grained side-effect system, which enables safe and aggressive operation reordering.

Instead of explicit effect chains (like in TurboFan), Turboshaft uses a **Produces/Consumes** model based on bitmasks.

### Effect Dimensions
Defined in `EffectDimensions` (within `operations.h`), these are the individual bits of side effects:
*   `load_heap_memory`: Reading from the JS heap.
*   `store_heap_memory`: Writing to the JS heap.
*   `load_off_heap_memory`: Reading from C++ memory or external resources.
*   `store_off_heap_memory`: Writing to C++ memory or external resources.
*   `control_flow`: Operation affects control flow (branches, throws, deopts).
*   `before_raw_heap_access` / `after_raw_heap_access`: Used to ensure consistency during raw heap operations (like inline allocation) where the heap might be temporarily invalid.

### OpEffects
Each operation defines its `OpEffects`, which consists of:
1.  **Produces**: An `EffectDimensions` bitmask of what effects this operation generates.
2.  **Consumes**: An `EffectDimensions` bitmask of what effects this operation depends on.
3.  **Other properties**:
    *   `can_create_identity`: True if operation produces a fresh identity (e.g., allocation). Prevents GVN and moving in/out of loops.
    *   `can_allocate`: True if operation can trigger GC.
    *   `required_when_unused`: True if operation should not be removed even if its result is unused (e.g., stores, side-effecting ops).

### Reordering Rules
Two operations **cannot** be reordered if the first operation **produces** an effect dimension that the second operation **consumes**.

**Example**:
*   `StoreOp` produces `store_heap_memory` and consumes `store_heap_memory` and `load_heap_memory`.
*   `LoadOp` produces `load_heap_memory` and consumes `store_heap_memory`.

If we have:
1.  `StoreOp(addr1, val)` (Produces `store_heap_memory`)
2.  `LoadOp(addr2)` (Consumes `store_heap_memory`)

They cannot be reordered because operation 1 produces an effect that operation 2 consumes.

However, if we have two loads:
1.  `LoadOp(addr1)` (Produces `load_heap_memory`, Consumes `store_heap_memory`)
2.  `LoadOp(addr2)` (Produces `load_heap_memory`, Consumes `store_heap_memory`)

Neither produces an effect that the other consumes (they both produce `load`, but consume `store`). Thus, they can be freely reordered.

This system allows Turboshaft to be very generous with reorderings when operations are high-level and pure, while automatically becoming more restrictive as operations are lowered to low-level memory accesses.

---

## See Also
-   `src/compiler/turboshaft/operations.h`: The source of truth for operations and effects.
-   [Turboshaft](compiler-turboshaft.md): Overview of the Turboshaft pipeline.
