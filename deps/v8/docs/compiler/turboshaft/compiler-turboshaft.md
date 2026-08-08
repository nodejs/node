# Turboshaft and Turbolev

This document covers V8's next-generation optimizing compiler pipeline, **Turboshaft**, and its frontend for Maglev graphs, **Turbolev**.

## Turboshaft: Next-Generation IR and Pipeline

Turboshaft is designed to replace the Sea-of-Nodes IR used in TurboFan. It introduces a more traditional Control Flow Graph (CFG) representation with explicit blocks and instructions.

### Key Features of Turboshaft
*   **CFG-Based IR**: Unlike TurboFan's Sea-of-Nodes, where control and data flow are intermingled, Turboshaft uses a clear separation. Blocks contain a linear sequence of operations.
*   **Reducers**: Optimizations are implemented as "Reducers" that can be composed. This makes it easier to write and combine optimization passes.
*   **Performance**: Designed to be faster to compile and generate better code than TurboFan.

### Common Reducers
Turboshaft includes many reducers for different optimization passes. Some examples include:
*   **MachineOptimizationReducer**: Performs low-level machine-specific optimizations.
*   **ValueNumberingReducer**: Implements global value numbering to eliminate redundant computations.
*   **TypeInferenceReducer**: Infers types for operations to enable further optimizations.
*   **BranchEliminationReducer**: Eliminates redundant branches based on known conditions.
*   **LoopUnrollingReducer**: Unrolls loops to reduce overhead.

## Motivation: Leaving the Sea of Nodes

Turboshaft was created to address several pain points with TurboFan's Sea-of-Nodes IR:

1.  **Complexity**: Sea-of-Nodes graphs are hard to read and reason about. Effect and control chains must be managed manually, leading to subtle bugs.
2.  **Limited Benefits for JS**: In JavaScript, most operations can have side effects or throw, meaning they must be on the effect or control chain. This defeats the purpose of Sea-of-Nodes, where operations are supposed to float freely.
3.  **Slow Compilation**: Finding a good order to visit nodes for optimizations is hard in a soup of nodes, leading to many revisits. Cache locality is also poor because nodes are mutated in place and new nodes are allocated arbitrarily.
4.  **Difficult Transformations**: Introducing new control flow during lowerings is hard because you have to figure out where to plug it into the control chain.

Turboshaft's return to a CFG IR solves these issues, resulting in simpler code and significantly faster compilation times.

For a more detailed discussion on why V8 adopted CFG and the benefits it brings, see [Why V8 Embraced CFG](../why-cfg.md).

---

## Turbolev: Bridging Maglev and Turboshaft

**Turbolev** (implemented in `src/compiler/turboshaft/turbolev-graph-builder.cc`) is a frontend for Turboshaft that constructs a Turboshaft graph directly from a **Maglev graph**.

### Rationale
Maglev is fast to compile and collects good type feedback, building a graph that is already somewhat optimized and has environment/frame state information attached. By starting from the Maglev graph instead of starting over from bytecode, Turboshaft can:
1.  Leverage the analysis and type inference already done by Maglev.
2.  Save time on graph building.

### Complex Graph Transformations

Translating from Maglev to Turboshaft is not always a 1-to-1 mapping because they have different IR constraints.

#### Example: Generator Resumes and Loop Dominance
A prime example of non-trivial translation is how Turbolev handles **Generators** (yield/resume).

*   **Maglev Constraint**: Maglev graphs for resumable generator functions can have edges that bypass loop headers (e.g., jumping directly into the middle of a loop from the initial switch-resume table).
*   **Turboshaft Constraint**: In Turboshaft, a loop header must strictly dominate every block in the loop. It does not allow arbitrary edges bypassing the loop header.
*   **Solution in Turbolev**: The `GeneratorAnalyzer` in `turbolev-graph-builder.cc` detects these loop header bypasses in the Maglev graph. Turbolev then transforms the graph by re-routing these resume edges to the loop headers and inserting secondary switches inside the loop to dispatch to the correct resume point.

## Turboshaft Pipeline

The general pipeline from IR to machine code in Turboshaft is as follows:

1.  **Graph Building**: The graph is built either from Ignition bytecode or by lowering from a higher-level representation (like Maglev via Turbolev).
2.  **Optimization Phases**: The graph passes through multiple reducer phases (dead code elimination, loop unrolling, machine optimization, etc.). This usually involves copying the graph from an input to an output graph while applying optimizations on the fly.
3.  **Scheduling**: The blocks are ordered, often using a special Reverse Post-Order (RPO) to ensure contiguous loop bodies.
4.  **Instruction Selection**: The `InstructionSelector` visits the operations and emits corresponding machine instructions into an `InstructionSequence`.
5.  **Register Allocation**: Registers are allocated for the values in the `InstructionSequence`.
6.  **Code Generation**: The finalized instruction sequence is translated into the actual machine code (bytes) for the target architecture.

## Instruction Selection

Instruction Selection is the phase that bridges the high-level Turboshaft IR to the low-level machine instructions.

*   **Shared Backend**: Turboshaft leverages the existing backend infrastructure used by TurboFan.
*   **Mapping to Instructions**: In `src/compiler/turboshaft/instruction-selection-phase.cc`, Turboshaft initializes an `InstructionSelector` specifically for Turboshaft graphs (`InstructionSelector::ForTurboshaft`). This selector traverses the Turboshaft IR and generates an `InstructionSequence`, mapping Turboshaft operations to architecture-specific machine instructions (defined in shared files like `instruction-selector-x64.cc`).

## File Structure
*   `src/compiler/turboshaft/`: Core Turboshaft implementation.
*   `src/compiler/turboshaft/turbolev-graph-builder.cc`: The main implementation of the Maglev-to-Turboshaft translator.
*   `src/compiler/turboshaft/turbolev-frontend-pipeline.cc`: The pipeline stages for the Turbolev frontend.
