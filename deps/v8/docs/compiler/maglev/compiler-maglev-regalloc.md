# Maglev Register Allocator (`StraightForwardRegisterAllocator`)

Maglev uses a fast, single-pass forward register allocator implemented by **`StraightForwardRegisterAllocator`** (`src/maglev/maglev-regalloc.h`, `src/maglev/maglev-regalloc.cc`). Unlike classic linear-scan or graph-coloring allocators that iterate to a fixed point or split live ranges globally, `StraightForwardRegisterAllocator` walks the CFG in topological RPO block order in a single forward pass, using precomputed live ranges and next-use chains to make greedy local eviction decisions.

---

## 1. Conceptual Overview: How It Works

Before `StraightForwardRegisterAllocator` runs, two pre-regalloc passes in `src/maglev/maglev-pre-regalloc-codegen-processors.h` prepare the graph:
1. **`ValueLocationConstraintProcessor`**: Calls `node->SetValueLocationConstraints()` on every node to initialize `compiler::UnallocatedOperand` constraints on all inputs (`ANY`, `MUST_HAVE_REGISTER`, `FIXED_REGISTER`, `FIXED_FP_REGISTER`), outputs (`REGISTER_OR_SLOT`, `SAME_AS_INPUT`, `FIXED_REGISTER`, `FIXED_SLOT`), and fixed/arbitrary temporaries.
2. **`LiveRangeAndNextUseProcessor`**: Assigns monotonically increasing `NodeIdT` IDs to every node in RPO order, extends lifetimes of outer-loop values across inner-loop `JumpLoop` backedges (`node->set_used_nodes(...)`), and builds a singly-linked chain of **`InputLocation`** uses for each `ValueNode` so `RegallocValueNodeInfo` knows its exact `live_range()` (`[start, end]`) and `current_next_use()`. It also computes per-loop `reload_hints_` and `spill_hints_` based on whether a loop-carried value is used in a register before the first call and after the last call of a loop.

With this information in place, the constructor of `StraightForwardRegisterAllocator` (`src/maglev/maglev-regalloc.cc`) executes three steps:
1. **`ComputePostDominatingHoles()`**: Links control-flow linear-ID liveness holes across branches in reverse RPO order.
2. **`AllocateRegisters()`**: Walks basic blocks and nodes forward, maintaining `RegisterFrameState<Register> general_registers_` and `RegisterFrameState<DoubleRegister> double_registers_`, inserting `GapMove` / `ConstantGapMove` nodes, assigning stack spill slots (`tagged_` and `untagged_`), and resolving control-flow merges.
3. **Frame Slot & OSR Alignment Finalization**: Adjusts `tagged_.top` and `untagged_.top` for OSR (`graph_->osr_values()`, `min_maglev_stackslots_for_unoptimized_frame_size()`) and ARM64 16-byte frame alignment, then records `graph_->set_tagged_stack_slots(tagged_.top)` and `graph_->set_untagged_stack_slots(untagged_.top)`.

---

## 2. Exact Per-Node Allocation Sequence (`AllocateNode`)

For each non-Phi, non-Dead node in a basic block, `StraightForwardRegisterAllocator::AllocateNode(Node* node)` (`src/maglev/maglev-regalloc.cc`) executes a strict 6-step sequence:

```cpp
// src/maglev/maglev-regalloc.cc
void StraightForwardRegisterAllocator::AllocateNode(Node* node) {
  current_node_ = node;
  // 1. Assign all input operands and temporaries (5 passes).
  AssignInputs(node);
  VerifyInputs(node);

  // 2. If the node is a call, spill and clear all registers BEFORE result allocation.
  if (node->properties().is_call()) SpillAndClearRegisters();

  // 3. Allocate the node's output register/slot (if it is a ValueNode).
  if (node->Is<ValueNode>()) AllocateNodeResult(node->Cast<ValueNode>());

  // 4. Allocate eager deopt input locations (if node can_eager_deopt()).
  if (node->properties().can_eager_deopt()) {
    AllocateEagerDeopt(*node->eager_deopt_info());
  }

  // 5. Allocate lazy deopt input locations and exception catch spills (if can_lazy_deopt()).
  if (node->properties().can_lazy_deopt()) {
    AllocateLazyDeopt(*node->lazy_deopt_info());
  }

  // 6. Save live register snapshot for deferred calls (if needs_register_snapshot()).
  if (node->properties().needs_register_snapshot()) {
    SaveRegisterSnapshot(node);
  }

  // Unblock registers and verify state at the end of the node.
  general_registers_.clear_blocked();
  double_registers_.clear_blocked();
  VerifyRegisterState();
}
```

### Step 1: `AssignInputs(node)` (Five-Pass Input & Temporary Ordering)
To prevent an unconstrained input or temporary from stealing a specific hardware register needed by a fixed-register input or temporary on the same instruction, `AssignInputs` executes five ordered passes (`src/maglev/maglev-regalloc.cc`):
1. **`AssignFixedInput(node)`**: Assigns inputs that require a specific hardware register (`FIXED_REGISTER` or `FIXED_FP_REGISTER`). Calls `ForceAllocate(reg, input_node)` (evicting or spilling any other value currently holding `reg`, emitting a `GapMove` into `reg`, and marking `reg` as **blocked** for the duration of the instruction) and advances the input's use chain via `UpdateUse(input)`.
2. **`AssignFixedTemporaries(node)`**: Frees and blocks any specific hardware registers requested in `node->general_temporaries()` and `node->double_temporaries()`.
3. **`AssignArbitraryRegisterInput(node)`**: Assigns inputs that require *any* register (`MUST_HAVE_REGISTER`). If the input value is already in a register (`TryChooseInputRegister`), that register is reused and blocked; otherwise `AllocateRegister` picks a free unblocked register (or evicts an unblocked register via `FreeUnblockedRegister` / `PickRegisterToFree`), emits a reload `GapMove`, and blocks the register.
4. **`AssignArbitraryTemporaries(node)`**: Allocates and blocks the count of arbitrary scratch registers requested in `node->num_temporaries_needed<RegisterT>()`.
5. **`AssignAnyInput(node)`**: Assigns inputs with `ANY` policy. If the value is already in a register, uses that register; otherwise uses its existing stack spill slot or constant operand directly without allocating a register.

Whenever `UpdateUse(input)` reaches the end of a `ValueNode`'s live range (`!info->has_next_live_use()`) and the node has no more uses (`IsCurrentNodeLastUseOf`), its registers are freed (or marked to be dropped at the end of the instruction) and its spill slot is returned to `free_slots` for reuse.

### Step 2: `if (node->properties().is_call()) SpillAndClearRegisters()`
If `node->properties().is_call()` is true (the node emits a runtime, builtin, or JS call that clobbers caller-saved registers), `SpillAndClearRegisters()` iterates over all currently used general and double registers, allocates a stack spill slot for any live value not yet spilled (`Spill(node)`), and clears the register file.
* Crucial ordering: This happens *after* `AssignInputs` (so call inputs can be passed in registers) and *before* `AllocateNodeResult` (so the call's return value register is not immediately spilled and cleared).

### Step 3: `AllocateNodeResult(ValueNode* node)` and `ApplyPatches`
Allocates the output location for a `ValueNode` based on its `UnallocatedOperand` policy:
* **`FIXED_SLOT`**: Used for `InitialValue` nodes (parameters and pre-allocated frame slots) and `RegisterInput`. Directly assigns the fixed stack slot or argument register and marks it as the node's spill slot.
* **`MUST_HAVE_REGISTER`**: Calls `AllocateRegisterAtEnd(node)`, which prefers a currently free unblocked register, or an input-blocked register whose input dies at `current_node_` (`IsCurrentNodeLastUseOf`), or else evicts a register via `PickRegisterToFree`. (`REGISTER_OR_SLOT` is `UNREACHABLE()` in `AllocateNodeResult`.)
* **`FIXED_REGISTER` / `FIXED_FP_REGISTER`**: Calls `ForceAllocate(reg, node)` (dropping any dying input in `reg` at the end of the node).
* **`SAME_AS_INPUT`**: Calls `ForceAllocate(node->input(operand.input_index()), node)`, forcing the output to share the exact hardware register assigned to that input. If that input was not yet in a register (e.g., it was a constant or spilled value) or its register had to be preserved for later uses, `ForceAllocate` inserts a `GapMove` before `current_node_` into a register and **patches** the input's `InputLocation` to point to that register.
* **`ApplyPatches`**: Because `AssignInputs` and `AllocateNodeResult` cannot mutate the block's `Node*` vector while iterating over it, any `GapMove` or `ConstantGapMove` emitted via `AddMoveBeforeCurrentNode` is queued in `patches_` (`GapMoveInfo`) and spliced into the basic block's node list at the end of the block.

### Step 4: `AllocateEagerDeopt(EagerDeoptInfo&)`
For nodes with `can_eager_deopt()` (such as `CheckMaps`, `CheckSmi`, `CheckedSmiUntag`), the deoptimizer can read values directly from either hardware registers or loadable stack/constant locations because an eager deopt jumps directly from inline code to the deoptimization entry stub without an intervening call.
* `AllocateEagerDeopt` walks every `ValueNode` in the `EagerDeoptInfo`'s `DeoptFrame` chain: if the value is currently in a register (`node_info->has_register()`) or already loadable from a constant or existing spill slot (`node_info->is_loadable()`), it uses `node_info->allocation()` directly without spilling; only if `!node_info->has_register() && !node_info->is_loadable()` does it call `Spill(node)` first, before calling `input->InjectLocation(node_info->allocation())` and `UpdateUse(node, input)`.

### Step 5: `AllocateLazyDeopt(LazyDeoptInfo&)`
For nodes with `can_lazy_deopt()` (calls that may deoptimize while deeper on the call stack when the call returns):
1. **Unconditional Spilling of Lazy Deopt Frame Inputs**: Because a call clobbers all caller-saved registers before returning to the lazy deopt continuation point, the deoptimizer can **only** reconstruct caller frame state from stack slots (or constants). Therefore, `AllocateLazyDeopt` calls **`Spill(node)`** on every non-constant `ValueNode` referenced by the `LazyDeoptInfo` frame chain and binds its `InputLocation` to `info->loadable_slot()`.
2. **Exception Handler Live-Value Spilling**: If the lazy-deopting node is inside a `try` block (`node->has_catch_block()`), an exception thrown by the call will unwind directly to `node->catch_block()`, where all registers are cleared (`ClearRegisters()`). `AllocateLazyDeopt` therefore inspects all values still in `general_registers_` and `double_registers_` and calls `Spill(node)` on any value that is live at the start of `catch_block`.

### Step 6: `SaveRegisterSnapshot(NodeBase* node)`
For nodes with `needs_register_snapshot()` (`OpProperties::DeferredCall()`, i.e., inline fast paths that jump to out-of-line `DeferredCodeInfo` blocks which make C++/builtin calls), registers are *not* spilled on the fast path. Instead, `SaveRegisterSnapshot` records a `RegisterSnapshot` on the node containing:
* `live_registers`: Bitmask of all general registers holding live values after the node's result is excluded (plus any registers holding `eager_deopt_info` inputs, in case the deferred code eagerly deopts after the call).
* `live_double_registers`: Bitmask of live FP registers.
* `live_tagged_registers`: Bitmask of live general registers whose `ValueNode` has `ValueRepresentation::kTagged`, so the deferred call's `SaveRegisterStateForCall` can register those pushed stack slots with the GC safepoint table.

---

## 3. Control-Flow Liveness Holes, `IsLiveAtTarget`, and `ComputePostDominatingHoles()`

Because `LiveRangeAndNextUseProcessor` approximates a node's live range as a single linear interval `[node->id(), last_use_id]`, a node `V` defined before a branch may appear "live" in a branch arm `B1` solely because its last use occurs in a sibling branch arm `B2` that has higher RPO `NodeIdT` numbers:

```
        [ Block 0: v1 = ... ]
             /         \
  [ Block 1 (id 10) ]  [ Block 2 (id 20): use(v1) ]
           |
       [ return ]
```

Here, `v1`'s linear `live_range().end` is `20`, so at Block 1 (`id 10`), `10 <= 20` even though Block 1 returns and never reaches Block 2. Similarly, a value's live range may span a region where the original node has no register and no spill slot because its value was temporarily converted into an alternative representation node.

`StraightForwardRegisterAllocator` handles these control-flow and representation liveness holes as follows (`src/maglev/maglev-regalloc.cc`):
1. **`ComputePostDominatingHoles()`**: Walks `BasicBlock*` in reverse RPO (`base::Reversed(*graph_)`) and links `control->set_next_post_dominating_hole(...)` on unconditional `JumpControlNode`s and `ConditionalControlNode`s using `NearestPostDominatingHole` and `HighestPostDominatingHole` (finding the lowest common post-dominating merge hole across branch arms) to chain control-flow linear-ID holes for `--trace-maglev-regalloc` live range visualization.
2. **`IsLiveAtTarget(ValueNode* node, BasicBlock* target)`**: Checks whether `node` is live on entry to `target`:
   * Returns `false` if `target` is a resumable loop header (since no registers can flow across resume entries).
   * On backward loop edges (`JumpLoop`, where `target->first_id() <= current_block_->first_id()`), checks `node->id() < target->FirstNonGapMoveId()` (values defined inside the loop are already dead at the backedge).
   * On forward edges, checks `node->live_range().end >= target->first_id()`.
3. **Dropping Dead/Hole Values at Merges and Loop Headers**: During `HoistLoopReloads` and `MergeRegisterValues`, if a node is inside a liveness hole or split across a conversion node such that it has neither a register nor a loadable slot (`!node_info->is_loadable() && !node_info->has_register()`), it cannot be reloaded or merged and is skipped/dropped from the merged register state.

---

## 4. Tagged vs. Untagged Spill Slots (`SpillSlots`)

`StraightForwardRegisterAllocator` maintains two completely separate stack slot allocators (`src/maglev/maglev-regalloc.h`):

```cpp
struct SpillSlots {
  int top = 0;
  // Sorted from earliest freed_at_position to latest freed_at_position.
  std::vector<SpillSlotInfo> free_slots;
};

SpillSlots untagged_;
SpillSlots tagged_;
```

### Why Separate `tagged_` and `untagged_` Regions?
When a garbage collection safepoint occurs during execution of a Maglev frame, the GC stack scanner must visit every stack slot that holds a tagged heap pointer (`ValueRepresentation::kTagged`) and must **never** interpret raw `Int32`, `Uint32`, `Float64`, `HoleyFloat64`, or `RawPtr` bits as heap pointers.

Instead of emitting a large per-safepoint bitvector describing the tag of every individual stack slot in the frame, Maglev partitions the spill area of `StandardFrame` into two contiguous regions:
1. **`tagged_` slots**: All values with `ValueRepresentation::kTagged`. The GC scans the contiguous `[0, tagged_.top)` region unconditionally at every safepoint.
2. **`untagged_` slots**: All values with `kInt32`, `kUint32`, `kFloat64`, `kHoleyFloat64`, `kIntPtr`, or `kRawPtr`. The GC completely ignores the `[0, untagged_.top)` region.

### Spill Slot Reuse (`--maglev-reuse-stack-slots`)
When a `ValueNode`'s last use is reached (`UpdateUse`), if the node had a spill slot allocated, that slot index and the node's death position (`node->live_range().end`) are inserted in sorted order into `tagged_.free_slots` or `untagged_.free_slots`.

When `AllocateSpillSlot(ValueNode* node)` needs a slot for a newly spilled node `N`:
* It binary-searches (`std::upper_bound`) the corresponding `free_slots` vector for a slot whose `freed_at_position < N->live_range().start` (ensuring `N`'s lifetime never overlaps with the previous occupant of that slot, even if `N` is spilled late in its lifetime).
* To prevent cycle-detection issues in the parallel move resolver (`GapResolver`), it also requires `it->double_slot == double_slot` so 64-bit float slots and general-purpose slots are not mixed in the same reused slot.
* If no compatible free slot exists, it bumps `slots.top`.

---

## 5. Control-Flow Merges & Loop Hint Hoisting

When finishing a basic block, `AllocateControlNode` propagates register state to each successor `target` via `MergeRegisterValues`:
* **First Predecessor (`InitializeBranchTargetRegisterValues`)**: If `target` has no `MergePointRegisterState` yet, it clones the current register file (filtering out nodes where `!IsLiveAtTarget`). If `target` is a loop header with precomputed `RegallocLoopInfo`:
  * `HoistLoopReloads(target, registers)` pre-loads loop-used values from `reload_hints_` into free registers *before* entering the loop header so they stay in registers across the loop.
  * `HoistLoopSpills(target)` forces values in `spill_hints_` out of registers before the loop header so they do not have to be repeatedly spilled inside the loop body and reloaded on the backedge.
* **Subsequent Predecessors (`MergeRegisterValues`)**: Compares each hardware register in `target`'s `MergePointRegisterState` against the incoming predecessor's register state. When predecessors disagree on which register holds a live value (or when merging a `Phi` whose representation fits in a register), `StraightForwardRegisterAllocator` allocates a `RegisterMerge` array attached to the `RegisterState` pointer (tagged via the low bit inside `RegisterStateFlags`), which the code generator's `ParallelMoveResolver` lowers into parallel register/stack moves on each incoming edge.
