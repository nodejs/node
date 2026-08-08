# TurboFan: V8's Optimizing Compiler

TurboFan is V8's top-tier optimizing compiler. It is responsible for generating the most efficient machine code possible for hot functions, utilizing all available type information and aggressive optimization techniques.

## Architecture

TurboFan has historically used a **"Sea of Nodes"** Intermediate Representation (IR). V8 has transitioned to a new IR framework called **Turboshaft**, which is now enabled by default for JavaScript. The current pipeline often uses a hybrid approach, starting with Sea of Nodes and lowering to Turboshaft for later phases.

### Sea of Nodes IR
In the Sea of Nodes representation, there is no strict control flow graph (CFG) during the early and middle optimization phases. Instead:
*   **Nodes** represent operations (e.g., additions, loads, constants).
*   **Edges** represent data flow, effect flow (ordering of operations with side effects), and control flow.
*   This representation allows the compiler to freely reorder operations as long as data and effect dependencies are respected, enabling powerful optimizations like code motion and redundant load elimination.

### Turboshaft
Turboshaft is the next-generation IR for V8. It uses a more structured control flow graph with basic blocks and linear sequences of operations within blocks. This makes certain optimizations easier to implement and reason about compared to the Sea of Nodes.

## The Compilation Pipeline

The compilation process is managed by the `Pipeline` class (defined in `src/compiler/pipeline.h`) and `turboshaft::Pipeline` (defined in `src/compiler/turboshaft/pipelines.h`). It consists of several phases, often spanning both IRs:

1.  **Graph Building**: The initial graph is built from Ignition bytecode or from Maglev IR in the Sea of Nodes representation.
2.  **Sea of Nodes Optimizations**: The graph goes through a series of optimization phases in the Sea of Nodes IR, including:
    *   **Inlining**: Replacing function calls with the body of the called function.
    *   **Escape Analysis**: Determining if objects allocated in the function escape to the heap.
    *   **Simplified Lowering**: Lowering high-level JavaScript operations to simpler machine-level operations based on type feedback.
    *   **Loop Peeling**: Peeling loops to enable further optimizations.
3.  **Turboshaft Conversion**: The optimized Sea of Nodes graph is converted to the Turboshaft IR.
4.  **Turboshaft Optimizations**: Further optimizations are performed in Turboshaft, such as:
    *   **Machine Lowering**: Lowering to machine-level operations.
    *   **Load Elimination**: Removing redundant loads.
    *   **Loop Unrolling**: Unrolling loops for better performance.
5.  **Instruction Selection**: Mapping the IR nodes to specific machine instructions for the target architecture.
6.  **Register Allocation**: Assigning physical registers to virtual registers.
7.  **Code Generation**: Emitting the final machine code.

## Deoptimization

TurboFan generates highly specialized code based on optimistic assumptions (e.g., assuming a specific hidden class). To ensure correctness, it inserts **checks** before specialized operations.

If a check fails at runtime (e.g., a function receives an object with a Map it hasn't seen before), the code cannot continue. TurboFan triggers a **Deoptimization (Deopt)**.

*   The current state of the optimized frame is translated back into an unoptimized frame compatible with Ignition or Sparkplug.
*   Execution resumes in the unoptimized tier.
*   The `FeedbackVector` is updated to reflect the new reality, so that a future re-optimization can take it into account.

## External Resources

### Articles and blog posts

- [A tale of TurboFan](https://benediktmeurer.de/2017/03/01/v8-behind-the-scenes-february-edition)
- [Ignition + TurboFan and ES2015](https://benediktmeurer.de/2016/11/25/v8-behind-the-scenes-november-edition)
- [An introduction to speculative optimization in V8](https://ponyfoo.com/articles/an-introduction-to-speculative-optimization-in-v8)
- [Sea of Nodes](https://darksi.de/d.sea-of-nodes/) (External blog post)
- [High-level overview of TurboFan](/blog/turbofan-jit) (V8 blog post)

### Talks

- [CodeStubAssembler: Redux](https://docs.google.com/presentation/d/1u6bsgRBqyVY3RddMfF1ZaJ1hWmqHZiVMuPRw_iKpHlY)
- [An overview of the TurboFan compiler](https://docs.google.com/presentation/d/1H1lLsbclvzyOF3IUR05ZUaZcqDxo7_-8f4yJoxdMooU/edit)
- [TurboFan IR](https://docs.google.com/presentation/d/1Z9iIHojKDrXvZ27gRX51UxHD-bKf1QcPzSijntpMJBM)
- [TurboFan’s JIT Design](https://docs.google.com/presentation/d/1sOEF4MlF7LeO7uq-uThJSulJlTh--wgLeaVibsbb3tc)
- [Fast arithmetic for dynamic languages](https://docs.google.com/a/google.com/presentation/d/1wZVIqJMODGFYggueQySdiA3tUYuHNMcyp_PndgXsO1Y)
- [Deoptimization in V8](https://docs.google.com/presentation/d/1Z6oCocRASCfTqGq1GCo1jbULDGS-w-nzxkbVF7Up0u0)
- [TurboFan: a new code generation architecture for V8](https://docs.google.com/presentation/d/1_eLlVzcj94_G4r9j9d_Lj5HRKFnq6jgpuPJtnmIBs88) ([video](https://www.youtube.com/watch?v=M1FBosB5tjM))
- [An internship on laziness](https://docs.google.com/presentation/d/1AVu1wiz6Deyz1MDlhzOWZDRn6g_iFkcqsGce1F23i-M) (+ [blog post](/blog/lazy-unlinking))

### Design documents

These are design documents that are mostly concerned with TurboFan internals.

- [Function context specialization](https://docs.google.com/document/d/1CJbBtqzKmQxM1Mo4xU0ENA7KXqb1YzI6HQU8qESZ9Ic)
- [Rest parameters and arguments exotic objects optimization plan](https://docs.google.com/document/d/1DvDx3Xursn1ViV5k4rT4KB8HBfBb2GdUy3wzNfJWcKM)
- [TurboFan developer tools integration](https://docs.google.com/document/d/1zl0IA7dbPffvPPkaCmLVPttq4BYIfAe2Qy8sapkYgRE)
- [TurboFan inlining](https://docs.google.com/document/d/1l-oZOW3uU4kSAHccaMuUMl_RCwuQC526s0hcNVeAM1E)
- [TurboFan inlining heuristics](https://docs.google.com/document/d/1VoYBhpDhJC4VlqMXCKvae-8IGuheBGxy32EOgC2LnT8)
- [TurboFan redundant bounds and overflow check elimination](https://docs.google.com/document/d/1R7-BIUnIKFzqki0jR4SfEZb3XmLafa04DLDrqhxgZ9U)
- [Lazy deoptimization without code patching](https://docs.google.com/document/d/1ELgd71B6iBaU6UmZ_lvwxf_OrYYnv0e4nuzZpK05-pg)
- [Register allocator](https://docs.google.com/document/d/1aeUugkWCF1biPB4tTZ2KT3mmRSDV785yWZhwzlJe5xY)
- [Projection nodes in TurboFan](https://docs.google.com/document/d/1C9P8T98P1T_r2ymuUFz2jFWLUL7gbb6FnAaRjabuOMY/edit)

### Related design documents

These are design documents that also affect TurboFan in a significant way.

- [Computed property names (re)design document](https://docs.google.com/document/d/1eH1R6_C3lRrLtXKw0jNqAsqJ3cBecrqqvfRzLpfq7VE)
- [ES2015 and beyond performance plan](https://docs.google.com/document/d/1EA9EbfnydAmmU_lM8R_uEMQ-U_v4l9zulePSBkeYWmY)
- [Iterator builtins design document](https://docs.google.com/document/d/13z1fvRVpe_oEroplXEEX0a3WK94fhXorHjcOMsDmR-8)
- [Making ES2015 classes fast](https://docs.google.com/document/d/1iCdbXuGVV8BK750wmP32eF4sCrnZ8y3Qlz0JiaLh9j8)
- [RegExp builtins (re)design document](https://docs.google.com/document/d/1MuqFjsfaRPL2ZqzVoeMRqtcAmcJSwmHljTbRIctVVUk)
- [Spread call performance](https://docs.google.com/document/d/1DWPizOSKqHhSJ7bdEI0HIVnner84xToEKUYqgXm3g30)

## See Also
-   [Ignition](../../interpreter/interpreter-ignition.md)
-   [Sparkplug](../sparkplug/compiler-sparkplug.md)
-   [Maglev](../maglev/compiler-maglev.md)
-   [Hidden Classes and Inline Caches](../../runtime/hidden-classes-and-ics.md)
