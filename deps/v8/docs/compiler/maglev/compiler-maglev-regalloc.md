# Maglev Register Allocator: Linear Scan and Constraints

This document provides a deep dive into Maglev's register allocator, explaining how it works and how IR nodes communicate their register requirements.

## The Allocator: `StraightForwardRegisterAllocator`

Maglev uses a linear scan register allocator called `StraightForwardRegisterAllocator` (declared in `src/maglev/maglev-regalloc.h` and implemented in `src/maglev/maglev-regalloc.cc`). It is designed to be fast and operates on a single IR level.

The allocator maintains the state of registers (free, used, blocked) using `RegisterFrameState`. It processes the graph block by block, and node by node within each block.

## How Nodes Register Their Needs

Before the actual register allocation pass, Maglev runs a pre-allocation phase called `ValueLocationConstraintProcessor` (in `src/maglev/maglev-pre-regalloc-codegen-processors.h`). This processor iterates over all nodes and calls their `SetValueLocationConstraints()` method.

In `SetValueLocationConstraints()`, a node specifies what kind of location (register, stack, or specific register) it requires for its inputs and where it wants its output to be placed.

### Constraint Helpers

Nodes use a set of inline helper functions (defined in `src/maglev/maglev-ir-inl.h`) to set these constraints. These helpers modify the `InputLocation` or result operand of the node, setting them as `compiler::UnallocatedOperand` with specific policies.

Here are the common helpers:

#### For Inputs:
*   `UseRegister(Input input)`: The input must be in a general-purpose register at the end of the node's execution.
*   `UseAndClobberRegister(Input input)`: The input must be in a register at the start, but that register may be clobbered by the node.
*   `UseAny(Input input)`: The input can be in a register, stack slot, or be a constant.
*   `UseFixed(Input input, Register reg)`: The input must be in the specific register `reg`.
*   `UseFixed(Input input, DoubleRegister reg)`: The input must be in the specific double register `reg`.
*   `UseAndClobberFixed(Input input, Register reg)`: The input must be in the specific register `reg` at the start, but that register may be clobbered by the node.

#### For Outputs (Results):
*   `DefineAsRegister(Node* node)`: The result will be placed in a general-purpose register.
*   `DefineAsFixed(Node* node, Register reg)`: The result will be placed in the specific register `reg`.
*   `DefineAsConstant(Node* node)`: The result is a constant and doesn't need a register.
*   `DefineSameAsFirst(Node* node)`: The result will be placed in the same register as the first input. This is common for operations that modify their first operand in-place (e.g., `addl` on x86).

### Example: `Int32AddWithOverflow`

Here is how `Int32AddWithOverflow` registers its needs in `src/maglev/x64/maglev-ir-x64.cc`:

```cpp
void Int32AddWithOverflow::SetValueLocationConstraints() {
  UseRegister(LeftInput());
  if (TryGetInt32ConstantInput(kRightIndex)) {
    UseAny(RightInput());
  } else {
    UseRegister(RightInput());
  }
  DefineSameAsFirst(this);
}
```

*   It requests a register for the `LeftInput`.
*   If the `RightInput` is a constant, it allows `UseAny` (it can be an immediate value in the instruction). Otherwise, it requests a register.
*   It uses `DefineSameAsFirst` because the x64 `add` instruction typically overwrites its first operand (`add eax, ebx`).

## The Allocation Process

When `StraightForwardRegisterAllocator::AllocateNode` runs, it processes these constraints in a specific order to minimize conflicts and spills:

1.  **Assign Inputs**: `AssignInputs(node)` handles input operands:
    *   **Fixed Inputs**: Processed first (e.g., `UseFixed`). These are rigid constraints.
    *   **Fixed Temporaries**: Registers needed as scratch space.
    *   **Arbitrary Register Inputs**: Processed next (e.g., `UseRegister`). The allocator tries to find free registers or spills values to make room.
    *   **Arbitrary Temporaries**: Non-fixed scratch registers.
    *   **Any Inputs**: Processed last (e.g., `UseAny`). These are the most flexible.
2.  **Allocate Node Result**: `AllocateNodeResult` handles the output constraint (e.g., `DefineSameAsFirst` or `DefineAsRegister`).
3.  **Allocate Deopts**: Handles inputs needed for eager and lazy deoptimization, ensuring they are loadable.

By separating constraint specification from the allocation algorithm, Maglev keeps node definitions clean while allowing the allocator to make globally informed decisions about register usage.

---

## See Also
-   `src/maglev/maglev-regalloc.h`: Register allocator declaration.
-   `src/maglev/maglev-regalloc.cc`: Register allocator implementation.
-   `src/maglev/maglev-ir-inl.h`: Constraint helper functions.
