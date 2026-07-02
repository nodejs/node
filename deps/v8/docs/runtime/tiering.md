# Tiering and the Interrupt Budget

This document explains how V8 manages the transition of functions between different execution tiers (Interpretation, Baseline, Mid-Tier, and Top-Tier optimization) and the mechanism used to trigger these transitions.

## Execution Tiers

V8 uses a multi-tiered execution strategy to balance startup time and peak performance:

1.  **Ignition (Interpreter)**: Executes bytecode. Fast startup, low memory overhead, but slower execution. Collects type feedback.
2.  **Sparkplug (Baseline Compiler)**: Compiles bytecode directly to machine code without optimizations. Very fast compilation, better performance than Ignition.
3.  **Maglev (Mid-Tier Compiler)**: Graph-based optimizing compiler. Fast compilation, generates high-quality code by utilizing type feedback.
4.  **TurboFan / Turboshaft (Top-Tier Compiler)**: Advanced optimizing compilers. Slow compilation, but generates the most highly optimized code for peak performance.

## The Interrupt Budget

To decide when to promote (or "tier up") a function to a higher tier, V8 uses an **Interrupt Budget** mechanism.

*   Each function has a budget associated with it, stored in its `FeedbackCell` (specifically `interrupt_budget_`).
*   The budget is initialized to a certain value (dependent on the current tier and flags).
*   As the function executes, the budget is decremented.
*   When the budget reaches zero or goes negative, V8 triggers a check to see if the function should be tiered up.

## Decrementing the Budget

All execution tiers (Ignition, Sparkplug, Maglev) decrement the interrupt budget at strategic points:

*   **Loop Back-Edges**: To detect hot loops.
*   **Function Returns**: To detect frequently called functions.

### Ignition (Interpreter)
In `src/interpreter/interpreter-assembler.cc`, Ignition decrements the budget in `DecreaseInterruptBudget` which is called by `JumpBackward` (for loops) and `UpdateInterruptBudgetOnReturn` (for returns).

### Sparkplug (Baseline)
In architecture-specific files like `src/baseline/x64/baseline-assembler-x64-inl.h`, Sparkplug decrements the budget in `AddToInterruptBudgetAndJumpIfNotExceeded` (using negative weights) at returns and loop back-edges.

### Maglev (Mid-Tier)
In Maglev IR, these are represented by nodes like `ReduceInterruptBudgetForLoop` and `ReduceInterruptBudgetForReturn`.

### Example (Maglev x64)

In `src/maglev/x64/maglev-ir-x64.cc`, the budget reduction is implemented by subtracting the amount from the `FeedbackCell`:

```cpp
void GenerateReduceInterruptBudget(MaglevAssembler* masm, Node* node,
                                   Register feedback_cell,
                                   ReduceInterruptBudgetType type, int amount) {
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  __ subl(
      FieldOperand(feedback_cell, offsetof(FeedbackCell, interrupt_budget_)),
      Immediate(amount));
  ZoneLabelRef done(masm);
  __ JumpToDeferredIf(less, HandleInterruptsAndTiering, done, node, type);
  __ bind(*done);
}
```

If the result of the subtraction is negative (`less`), it jumps to deferred code (`HandleInterruptsAndTiering`).

## Handling the Interrupt

When the budget is exceeded, V8 calls into the C++ runtime. Each tier has its own set of runtime functions:

*   **Ignition**: `Runtime::kBytecodeBudgetInterruptWithStackCheck_Ignition` and `Runtime::kBytecodeBudgetInterrupt_Ignition`.
*   **Sparkplug**: `Runtime::kBytecodeBudgetInterruptWithStackCheck_Sparkplug` and `Runtime::kBytecodeBudgetInterrupt_Sparkplug`.
*   **Maglev**: `Runtime::kBytecodeBudgetInterruptWithStackCheck_Maglev` and `Runtime::kBytecodeBudgetInterrupt_Maglev`.

The "WithStackCheck" variants are typically used at loop back-edges to handle both interrupts (e.g., GC requests, debugging pauses) and stack limit checks simultaneously.

All these runtime functions eventually call into the **`TieringManager`** via `TieringManager::OnInterruptTick`.

## The Decision to Tier Up

The `TieringManager` inspects the function's profiling data (feedback vector, invocation count, etc.) to make a decision:

*   It checks if the function is "hot" enough to warrant optimization.
*   It decides which tier to target (e.g., if Maglev is enabled and the function is currently in Sparkplug, it might target Maglev).
*   It queues the function for concurrent compilation in the background.

Once the background compilation finishes, V8 updates the function's entry in the **JS Dispatch Table**.

### JS Dispatch Table and Handles

In modern V8 (especially with the V8 Sandbox enabled), a `JSFunction` does not directly point to its `Code` object. Instead, it contains a `JSDispatchHandle` (an index into the `JSDispatchTable`).

*   **Indirection**: The `JSDispatchTable` holds the actual pointer to the `Code` (and the entry point).
*   **Update**: When tiering up, V8 does not modify the `JSFunction` object itself. Instead, it updates the entry in the `JSDispatchTable` pointed to by the function's handle.
*   **Atomicity**: This allows atomic updates to the code entry point, ensuring thread safety and facilitating concurrent execution and tiering without complex locking on the function object itself.

Future calls to the function will automatically use the new code because they go through the same dispatch handle.

## On-Stack Replacement (OSR)

OSR is a technique that allows V8 to switch execution from unoptimized code (Ignition) to optimized code (Maglev/TurboFan) **while the function is currently executing** (on the stack).

### Why it's needed
If a function contains a very long-running loop but is otherwise not called frequently, the budget interrupt at the return point will not trigger optimization soon enough. The loop back-edge budget reduction handles this by triggering optimization while still in the loop.

### How it works
1.  **Detection**: When a loop back-edge budget reaches zero, V8 checks if OSR is appropriate.
2.  **Compilation**: V8 compiles the function with Maglev or TurboFan, but targeting a specific loop header as the entry point instead of the normal function start.
3.  **Replacement**: The interpreter stack frame is replaced with an optimized stack frame (frame reconstruction), and execution continues in the optimized code at the loop header.

---

## See Also
-   `src/maglev/maglev-compiler.cc`: Maglev compilation pipeline.
-   `src/execution/tiering-manager.cc`: V8's tiering manager.
