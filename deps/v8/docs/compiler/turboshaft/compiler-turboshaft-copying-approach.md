# Turboshaft's Copying Approach, `SnapshotTable`, and `VariableReducer`

Instead of mutating a single IR graph in place during optimization passes, Turboshaft's **`CopyingPhase<Reducers...>`** (`src/compiler/turboshaft/copying-phase.h`) transforms an immutable **`input_graph`** into a fresh **`output_graph`** in a single dominator-tree traversal.

This out-of-place copying architecture allows a single pass to combine CFG structural transformations (block inlining, branch elimination, loop unrolling/peeling, multi-block lowering) with path-sensitive dataflow analyses (`SnapshotTable`) and on-the-fly SSA reconstruction (`VariableReducer`).

---

## 1. Conceptual Overview: How `CopyingPhase` Works

Every `Graph` lazily owns a companion `Graph` (`input_graph.GetOrCreateCompanion()`). When `CopyingPhase<Reducers...>::Run` executes (`src/compiler/turboshaft/copying-phase.h`):

1. **`GraphVisitor`** resets `output_graph` (reusing its already-allocated `OperationBuffer` memory without freeing it) and pre-allocates a 1-to-1 initial `block_mapping_` from every `input_block` in `input_graph` to a fresh `Block` in `output_graph`.
2. **`VisitAllBlocks()`** traverses the `input_graph` blocks in **dominator-tree order** (`StartBlock()`, pushing children via `block->LastChild()` / `NeighboringChild()`).
3. For each reachable block, **`VisitBlockBody`** walks its operations in `input_graph`, lowers/optimizes them through the `Assembler<GraphVisitor, Reducers...>` stack into `output_graph`, and records the mapping from each old `OpIndex` to its new `OpIndex` in **`op_mapping_`** (or in **`VariableReducer`** if the block is cloned).
4. At the end of the phase, **`Finalize()`** updates `NodeOriginTable` and calls **`input_graph_.SwapWithCompanion()`**, making the newly built `output_graph` the active `input_graph` for the next phase.

---

## 2. Why `VisitOpNoMappingUpdate` Skips `MayThrow` Operations

One of the most important (and initially surprising) lines in `GraphVisitor::VisitOpNoMappingUpdate` (`src/compiler/turboshaft/copying-phase.h:665-673`) is:

```cpp
// src/compiler/turboshaft/copying-phase.h
switch (op.opcode) {
#define EMIT_INSTR_CASE(Name)                                             \
  case Opcode::k##Name:                                                   \
    if (MayThrow(Opcode::k##Name)) return OpIndex::Invalid();             \
    new_index = Asm().ReduceInputGraph##Name(index, op.Cast<Name##Op>()); \
    break;
  TURBOSHAFT_OPERATION_LIST(EMIT_INSTR_CASE)
#undef EMIT_INSTR_CASE
}
```

### Why Does `VisitOpNoMappingUpdate` Return `OpIndex::Invalid()` for `MayThrow` Ops?

As explained in [Turboshaft IR (`compiler-turboshaft-ir.md`)](./compiler-turboshaft-ir.md#5-throwing-operations--throwing_op_boilerplate), any operation in `TURBOSHAFT_THROWING_OPERATIONS_LIST` (`CallOp`, `FastApiCallOp`, `GenericBinopOp`, `GenericUnopOp`, `ToNumberOrNumericOp`, `LoadDictionaryFieldOp`, etc., for which `MayThrow(opcode)` is `true`) is part of a 3-instruction sequence in the input graph:

```
  [ Block B0 ]
    o10: CallOp(...)  / GenericBinopOp(...)         <-- 1. Skipped by VisitOpNoMappingUpdate!
    o11: CheckExceptionOp(o10, didnt_throw: B1, catch: B2)  <-- 2. Block terminator of B0
          |
          v
  [ Block B1 (didnt_throw_block, single predecessor B0) ]
    o12: DidntThrowOp(throwing_operation: o10)      <-- 3. Inlined inside CatchScope!
    o13: ... rest of B1 ...
```

If `VisitOpNoMappingUpdate` reduced `o10` (`CallOp` / `GenericBinopOp`) when visiting `o10`:
1. At `o10`, `GraphVisitor` has **not yet reached `o11` (`CheckExceptionOp`)**, so it does not yet know what `op.catch_block` (or `op.effect_handlers`) is attached to this throwing operation! If a reducer lowered `o10` into a subgraph containing one or more new throwing calls, those calls would be emitted *outside* any `CatchScope` and would fail to wire edges to `catch_block`.
2. Furthermore, consumers of `o10`'s result never reference `o10`'s `OpIndex` anyway; they reference **`o12` (`DidntThrowOp`)**.

Therefore, `GraphVisitor` handles the trio as follows:
1. **At `o10` (the `MayThrow` op)**: `VisitOpNoMappingUpdate` immediately returns `OpIndex::Invalid()`, doing nothing.
2. **At `o11` (`CheckExceptionOp`, the block terminator of `B0`) OR at `o12` (`DidntThrowOp`, if `o10` had no `CheckExceptionOp`)**:
   * If `o10` was followed by `CheckExceptionOp` (`AssembleOutputGraphCheckException`, lines 913-972):
     1. Opens a **`CatchScope(Asm(), MapToNewGraph(op.catch_block))`** (if `op.catch_block != nullptr`).
     2. Calls `Asm().InlineOp(*it, op.didnt_throw_block)` on the very first operation of `didnt_throw_block`, which is guaranteed (`DCHECK`) to be **`o12` (`DidntThrowOp`)**.
     3. Inside that `CatchScope`, `AssembleOutputGraphDidntThrow(const DidntThrowOp& op)` looks up `op.throwing_operation()` (`o10`) in `input_graph` and invokes **`Asm().ReduceInputGraph<Name>(op.throwing_operation(), throwing_operation.Cast<NameOp>())`**!
     4. Any throwing operations emitted by the reduction of `o10` automatically see the active `CatchScope`, wire their exception edges to `catch_block`, bind a new `didnt_throw_block` in `output_graph`, and return the new `DidntThrowOp`'s `OpIndex` as the reduction result of `o12` (`CreateOldToNewMapping(o12, result)`).
     5. Finally, `AssembleOutputGraphCheckException` closes the `CatchScope` and inlines the remaining operations of `op.didnt_throw_block` (`o13...`) into the current output block.
   * If `o10` was *not* inside a `try` block (so `o10` was directly followed by `o12: DidntThrowOp` in the same block without a `CheckExceptionOp`), visiting `o12` calls `AssembleOutputGraphDidntThrow`, which triggers `ReduceInputGraph<Name>(o10, ...)` and maps `o12` to the resulting `OpIndex`.

---

## 3. `SnapshotTable`: O(1) Lookup with LCA Rollback & Replay

Many Turboshaft reducers (`VariableReducer`, `LateLoadEliminationReducer`, `BranchEliminationReducer`, `ValueNumberingReducer`, and Maglev's `MaglevPhiRepresentationSelector`) need a key-value map that tracks state along the CFG paths of the dominator tree and merges state at control-flow join points.

Cloning a hash table at every basic block would take $O(\text{blocks} \times \text{keys})$ time and memory. Instead, **`SnapshotTable<Value, KeyData>`** (`src/compiler/turboshaft/snapshot-table.h`) maintains a **single mutable table** (`ZoneDeque<TableEntry> table_`) paired with an **append-only undo/redo log** (`ZoneVector<LogEntry> log_`).

### Data Structures & Complexity
* **`Key` (`SnapshotTableKey<Value, KeyData>`)**: Wraps a direct pointer `TableEntry* entry_` into `table_`.
  * **`Get(Key key)`** is a direct pointer dereference `key.entry_->value`: **`O(1)`**.
  * **`Set(Key key, Value new_value)`** checks `if (key.entry_->value == new_value) return false;`, appends `LogEntry{*key.entry_, old_value, new_value}` to `log_`, and overwrites `key.entry_->value = new_value`: **`O(1)`**.
* **`SnapshotData`**: Represents a sealed basic block state as a node in the snapshot tree (`SnapshotData* parent`, `uint32_t depth`, and slice `[log_begin, log_end)` in `log_`).
  * **`Seal()`**: Records `log_end = log_.size()`. If `log_begin == log_end` (the block made no modifications to the table), `Seal()` discards the empty snapshot and returns its `parent` snapshot directly (`O(1)`).

### Switching Snapshots via Lowest Common Ancestor (`MoveToNewSnapshot`)
Consider switching from sealed snapshot `S2` to a new merge block whose CFG predecessors are `S5` and `S6`:

```
          S0
        /    \
       S1     S3
       |        \
       S2        S4   <-- common_ancestor of S5 and S6
      (curr)    /  \
               S5  S6 <-- predecessors of new block
```

When `StartNewSnapshot({S5, S6}, merge_fun)` is called (`src/compiler/turboshaft/snapshot-table.h:427-516`):
1. **Find Predecessor LCA (`common_ancestor`)**: Walks `parent` pointers using `depth` to find the Lowest Common Ancestor of all `predecessors` (`S5` and `S6` -> **`S4`**). `S4` will be the tree `parent` of the new snapshot.
2. **Find Rollback LCA (`go_back_to`)**: Finds the Lowest Common Ancestor of `common_ancestor` (`S4`) and `current_snapshot_` (`S2`), which is **`S0`**.
3. **Rollback (`RevertCurrentSnapshot`)**: Walks up from `current_snapshot_` (`S2 -> S1 -> S0`), iterating each snapshot's `LogEntries` in reverse and restoring `entry.table_entry.value = entry.old_value`.
4. **Replay (`ReplaySnapshot`)**: Walks down from `S0` to `common_ancestor` (`S0 -> S3 -> S4`), applying each snapshot's `LogEntries` forward (`entry.table_entry.value = entry.new_value`).
5. **Merge Divergent Entries (`MergePredecessors`)**: For each predecessor (`S5`, `S6`), walks the log entries between that predecessor and `common_ancestor` (`S4`), collecting modified `TableEntry*` pointers into `merging_entries_` and their per-predecessor values into `merge_values_` (pre-filled with `S4`'s value for predecessors that did not touch the key). Finally, calls `merge_fun(Key{*entry}, values)` for each modified key and records the merged value in the new snapshot's log!

Because `GraphVisitor` visits blocks in dominator-tree order, `current_snapshot_` and `common_ancestor` are almost always adjacent in the snapshot tree, making snapshot switching proportional only to the number of `Set()` calls since the immediate dominator.

---

## 4. Block Cloning, `VariableReducer`, and Loop Phis

In standard SSA copying (when each `input_block` is emitted at most once into `output_graph`), every operation `old_index` in `input_graph` maps to a single `new_index` in `output_graph`, stored in the flat array **`FixedOpIndexSidetable<OpIndex> op_mapping_`**. Because `old_index` dominates all of its uses (except loop phi backedges), `MapToNewGraph(old_index)` simply returns `op_mapping_[old_index]`.

### Why Block Cloning Breaks 1-to-1 `op_mapping_`
Several Turboshaft optimizations clone basic blocks from `input_graph` so they are emitted **multiple times** into `output_graph`:
* **Auto-Inlining Single-Predecessor Goto Targets (`VisitBlockTerminator` / `InlineWaitingBlock`)**: When a block ends with `Goto(destination)` and `destination->PredecessorCount() == 1`, `GraphVisitor` inlines `destination` directly into the current block.
* **`CloneBlockAndGoto(input_block)`** and **`CloneAndInlineBlock(input_block)`**: Used by `BranchEliminationReducer` and `MachineOptimizationReducer` to duplicate a join block into its predecessor(s) so a `PhiOp` becomes a known constant or branch condition.
* **`CloneSubGraph(sub_graph, ...)`**: Used by `LoopPeelingReducer` and `LoopUnrollingReducer` to emit a loop body multiple times.

When an `input_block` `B` is cloned (or may be visited more than once):
1. An operation `old_op` inside `B` is emitted multiple times, producing *different* `OpIndex` values (`new_op_1`, `new_op_2`, ...) in different output blocks!
2. If those cloned paths later reconverge at a downstream merge block `M`, any operation in `M` that uses `old_op` cannot use a single `op_mapping_[old_op]`; it needs a newly synthesized **`PhiOp(new_op_1, new_op_2)`** at `M`!

### How `GraphVisitor` + `VariableReducer` Solve This Automatically
`GraphVisitor` (`src/compiler/turboshaft/copying-phase.h`) and `VariableReducer` (`src/compiler/turboshaft/variable-reducer.h`) handle 1-to-many SSA reconstruction seamlessly:

1. **Marking `blocks_needing_variables_`**:
   Whenever `CloneBlockAndGoto`, `CloneAndInlineBlock`, `CloneSubGraph`, or `InlineWaitingBlock` clones/inlines an `input_block`, it adds `input_block->index().id()` to `BitVector blocks_needing_variables_` and sets **`current_block_needs_variables_ = true`**.
2. **`CreateOldToNewMapping(old_index, new_index)`**:
   * When `current_block_needs_variables_` is `false` (normal 1-to-1 block), it writes `op_mapping_[old_index] = new_index`.
   * When `current_block_needs_variables_` is `true`, it skips `op_mapping_` and instead looks up or allocates a **`Variable`** in **`old_opindex_to_variables[old_index]`** (`Asm().NewLoopInvariantVariable(rep)`) and calls **`Asm().SetVariable(*var, new_index)`**, which records `new_index` in `VariableReducer`'s `SnapshotTable` for the current output block!
3. **`MapToNewGraph(old_index, predecessor_index)`**:
   * First checks `OpIndex result = op_mapping_[old_index]`.
   * If `!result.valid()`, it looks up `MaybeVariable var = GetVariableFor(old_index)` in `old_opindex_to_variables` and queries `Asm().GetVariable(var.value())` (or `Asm().GetPredecessorValue(var.value(), predecessor_index)` when mapping `PhiOp` inputs).
   * Whenever `VariableReducer::Bind(new_block)` starts a merge block in `output_graph`, its `SnapshotTable` `merge_variables` callback automatically emits a **`PhiOp`** (or recursively merges `FrameStateOp`s via `MergeFrameState`) for any `Variable` whose incoming predecessor snapshots hold different `OpIndex` values!

### Two-Step Loop Phi Construction: `PendingLoopPhiOp` -> `FixLoopPhis`
Because Turboshaft visits blocks in dominator-tree order, a loop header `H` is always visited **before** its loop body and backedge block `B`. Consequently, when emitting a loop `PhiOp` in `H`, its forward-edge input (index 0) has already been emitted, but its backedge input (index 1, coming from `B`) **does not exist in `output_graph` yet**.

Turboshaft resolves this cycle in two steps:
1. **At the Loop Header (`ResolvePhi` / `VariableReducer::Bind`)**:
   * For explicit `PhiOp`s in `input_graph`, `GraphVisitor::ResolvePhi` maps the forward edge (`op.forward_edge()`) and emits a 1-input **`PendingLoopPhiOp(og_index, rep)`** into the output loop header.
   * For active non-loop-invariant `Variable`s in `VariableReducer::Bind(new_block)`, `VariableReducer` similarly emits a `PendingLoopPhiOp` for each variable in `table_.active_loop_variables` and records `{var, pending_loop_phi}` in `loop_pending_phis_[new_block->index()]`.
2. **At the Loop Backedge (`AssembleOutputGraphGoto` / `VariableReducer::ReduceGoto`)**:
   * When `GraphVisitor::AssembleOutputGraphGoto` encounters a backedge `GotoOp` (`op.is_backedge == true`), it **first** calls **`FixLoopPhis(op.destination)`** *before* calling `Asm().ReduceGoto(destination, true)` (because `ReduceGoto` may alter the `SnapshotTable` state).
   * `FixLoopPhis` iterates over the `PhiOp`s of the input loop header, calls `Asm().FixLoopPhi(*input_phi, phi_index, output_graph_loop)` to map the backedge input `input_phi->back_edge()` in the current backedge block's context, and uses **`output_graph().Replace<PhiOp>(phi_index, {forward_edge, back_edge}, rep)`** (`OperationBuffer::ReplaceScope`) to overwrite the `PendingLoopPhiOp` in place with a 2-input `PhiOp`.
   * Similarly, `VariableReducer::ReduceGoto` iterates `loop_pending_phis_[destination->index()]` and replaces each variable's `PendingLoopPhiOp` in place with `PhiOp({pending_phi.first(), GetVariable(var)}, pending_phi.rep)`.
   * **Note**: A `PendingLoopPhiOp` occupies the same number of `OperationStorageSlot`s (2 slots = 16 bytes) as a 2-input `PhiOp`, so overwriting it in place in `OperationBuffer` is always guaranteed to fit.
