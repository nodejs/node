# Maglev: V8's Mid-Tier Optimizing Compiler

Maglev is V8's mid-tier optimizing compiler. It is designed to be a fast optimizing compiler that provides a significant performance boost over Sparkplug while compiling much faster than TurboFan.

## Design Philosophy

Maglev is designed to be a "fast" optimizing compiler. It fills the gap between Sparkplug (very fast, no optimizations) and TurboFan (slow, highly optimized). To achieve this, Maglev makes several deliberate design choices:

1.  **Minimize Phases**: Unlike TurboFan, which has dozens of optimization and lowering phases, Maglev keeps the pipeline as short as possible.
2.  **Single IR**: Maglev uses a single level of Intermediate Representation (IR) from graph building to code generation. There are no multiple tiers of lowering (e.g., JS -> Simplified -> Machine).
3.  **Direct Code Generation**: IR nodes generate machine code directly via the `MacroAssembler`, without a separate instruction selection phase.

## Architecture

Maglev is a **graph-based compiler** that uses a Single Static Assignment (SSA) style Intermediate Representation (IR).

*   **Maglev IR**: The graph consists of nodes representing operations and edges representing data and control flow.
*   **Type Inference**: Maglev makes use of the type feedback collected by Ignition in the `FeedbackVector`. It uses this feedback to speculate on types and generate specialized code (e.g., assuming a property access is monomorphic).
*   **Register Allocation**: Maglev uses a linear scan register allocator to assign physical registers to IR nodes. See [Maglev Register Allocation](compiler-maglev-regalloc.md) for details.

## The Pipeline (Minimizing Phases)

The entire compilation pipeline in `src/maglev/maglev-compiler.cc` consists of only a few major steps:

1.  **Graph Building**: `MaglevGraphBuilder` iterates over bytecode and builds the graph in a single pass, performing abstract interpretation and inserting checks based on type feedback.
2.  **Inlining**: Small functions are inlined if enabled.
3.  **Optimization Passes**: A few selected, fast passes (Truncation propagation, Loop optimization, Phi representation selection).
4.  **Dead Code Marking**: Identifies used nodes and propagates usage information.
5.  **Pre-Register Allocation Processing**: Sweeps dead nodes, collects value location constraints, and computes live ranges.
6.  **Register Allocation**: A fast linear scan allocator (`StraightForwardRegisterAllocator`).
7.  **Code Assembly**: `MaglevCodeGenerator` iterates over the graph and emits code to a buffer.

## Graph Building

The core of Maglev's frontend is the `MaglevGraphBuilder` (defined in `src/maglev/maglev-graph-builder.h`).

1.  **Bytecode Iteration**: Like Sparkplug, it iterates through the `BytecodeArray`.
2.  **Abstract Interpretation**: It maintains an `InterpreterFrameState` which maps Ignition's virtual registers and accumulator to Maglev IR nodes (`ValueNode`).
3.  **Node Creation**: For each bytecode, it creates corresponding IR nodes and links them into the graph. For example, an `Add` bytecode might create an `Int32AddWithOverflow` node if feedback suggests integer addition.
4.  **Feedback Usage**: It queries the `JSHeapBroker` to read feedback and make decisions about inlining and specialized operations.

## Single IR Level

In TurboFan, a graph starts with high-level JavaScript operators, gets lowered to "Simplified" operators (handling types like numbers and strings), and finally to "Machine" operators (raw pointers, integers).

Maglev bypasses this multi-tiered lowering. Its IR nodes represent operations that are already close to machine level but still retain enough high-level information to support deoptimization.

For example, `Int32AddWithOverflow` is a single node that represents an integer addition that might fail and trigger a deopt. It doesn't need to be lowered further; it knows how to generate the code to perform the addition and check the overflow flag.

## Direct Code Generation

The most striking difference between Maglev and TurboFan is how they generate machine code.

In TurboFan, the graph is passed to an **Instruction Selector**, which matches patterns of nodes to machine instructions, creating a new "Instruction" list, which is then scheduled and colored by the register allocator.

In Maglev, after register allocation, the `MaglevCodeGenerator` simply iterates through the basic blocks and the nodes within them. Each node implements a `GenerateCode` method.

Here is an example from `src/maglev/x64/maglev-ir-x64.cc` for `Int32AddWithOverflow`:

```cpp
void Int32AddWithOverflow::GenerateCode(MaglevAssembler* masm,
                                        const ProcessingState& state) {
  Register left = ToRegister(LeftInput());
  if (!RightInput().operand().IsRegister()) {
    auto right_const = TryGetInt32ConstantInput(kRightIndex);
    DCHECK(right_const);
    __ addl(left, Immediate(*right_const)); // Emits x64 'add'
  } else {
    Register right = ToRegister(RightInput());
    __ addl(left, right); // Emits x64 'add'
  }
  // None of the mutated input registers should be a register input into the
  // eager deopt info.
  DCHECK_REGLIST_EMPTY(RegList{left} &
                       GetGeneralRegistersUsedAsInputs(eager_deopt_info()));
  // Emit eager deopt if overflow flag is set
  __ EmitEagerDeoptIf(overflow, DeoptimizeReason::kOverflow, this);
}
```

The `__` macro expands to `masm->`, which is the `MaglevAssembler` (inheriting from `MacroAssembler`). The node directly emits the `addl` instruction and the conditional jump for deoptimization.

This direct approach eliminates the overhead of instruction selection and intermediate lists, making compilation extremely fast.

## Deoptimization Support

A key requirement for Maglev is the ability to deoptimize back to the interpreter. This means that even though Maglev IR nodes are close to machine level, they must retain enough information to reconstruct the interpreter state.

Maglev achieves this by attaching `EagerDeoptInfo` or `LazyDeoptInfo` to nodes that can fail or side-effect.
-   **Eager Deoptimization**: Happens when a check fails (e.g., type check, overflow). The execution jumps to a deopt exit, which uses the `EagerDeoptInfo` to reconstruct the frame state at the point of failure.
-   **Lazy Deoptimization**: Happens when a call returns and the environment has changed in a way that makes the optimized code invalid.

The `DeoptFrame` stored in these info structures captures the state of the interpreter frames (parameters, registers, accumulator) at that specific point in the execution. This allows the deoptimizer to translate the current machine state back into interpreter frames.

## Optimizations

Maglev performs a curated set of optimizations that are fast to execute:

*   **Inlining**: Small, hot functions can be inlined.
*   **Type Guarding**: Inserts checks to ensure speculated types are correct at runtime, deoptimizing if they fail.
*   **Smi Untagging/Retagging**: Optimizes operations on small integers by removing and restoring the Smi tag only when necessary.

## Placement in the Pipeline

Maglev sits between Sparkplug and the optimizing compilers (TurboFan and Turboshaft). It is triggered when a function becomes hot enough, but before it is deemed worthy of the full optimization power (and compilation time) of TurboFan or Turboshaft.

## Turbolev

In the new Turboshaft pipeline, Maglev can also be used as a frontend. In this mode (known as **Turbolev**, enabled with `--turbolev`), Maglev builds the graph and performs initial optimizations, which is then lowered into Turboshaft IR for further optimization and code generation. This allows reusing Maglev's fast graph building and speculatively optimized IR as a starting point for Turboshaft.

## See Also
-   [Why V8 Embraced CFG](../why-cfg.md)
-   [Ignition](../../interpreter/interpreter-ignition.md)
-   [Sparkplug](../sparkplug/compiler-sparkplug.md)
-   [Hidden Classes and Inline Caches](../../runtime/hidden-classes-and-ics.md)
