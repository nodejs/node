# Why Control-Flow Graph (CFG) over Sea of Nodes?

When TurboFan was originally designed in 2013-2014, it adopted the **Sea-of-Nodes (SoN)** intermediate representation (`src/compiler/turbofan-graph.h`, `src/compiler/node.h`), where instructions float outside basic blocks and are constrained only by explicit **Value**, **Effect**, and **Control** edges.

Both of V8's newer optimizing compiler architectures, **Maglev** (`src/maglev/`) and **Turboshaft** (`src/compiler/turboshaft/`), deliberately moved away from Sea of Nodes in favor of explicit **Static Single Assignment (SSA) Control-Flow Graph (CFG)** representations. This document explains why Sea of Nodes became a bottleneck for JavaScript compilation and how Maglev and Turboshaft's concrete CFG designs solve those problems.

---

## 1. The Promise vs. Reality of Sea of Nodes

### What Sea of Nodes Was Designed to Do
In a classic Sea-of-Nodes graph, pure value computations (like `Int32Add(a, b)`) have no control or effect inputs. In theory, this makes global value numbering (GVN), constant folding, and loop-invariant code motion "free": because pure nodes are not pinned to a basic block, they float in a dependency DAG until a late `Scheduler` pass (`src/compiler/scheduler.cc`) places them into basic blocks.

### Why Sea of Nodes Struggles in V8
1. **JavaScript Operations Are Rarely Pure**:
   Unlike C or Fortran arithmetic loops, high-level JavaScript bytecodes almost always have **control** and **effect** dependencies: they check maps (`CheckMaps`), check Smi tags (`CheckSmi`), read mutable heap properties, allocate objects, or may throw exceptions or deoptimize. Consequently, the vast majority of nodes in a JavaScript graph are pinned to the effect and control chains anyway. Paying the memory and traversal cost of a 3-graph representation (Value + Effect + Control) when almost everything is control/effect-pinned is a poor trade-off.
2. **Manual Effect & Control Chain Maintenance Is Error-Prone**:
   Every in-place graph reducer in Sea of Nodes must manually rewire `EffectPhi`, `Phi`, and `Control` edges when replacing, splitting, or removing a node (`ReplaceWithValue`). Subtle bugs where an effect edge is bypassed or a control diamond is left half-wired have historically been a major source of security and correctness bugs in `src/compiler/`.
3. **Catastrophic Cache Locality and Pointer Chasing**:
   In Sea of Nodes, every `Node` is a separately Zone-allocated object with out-of-line input/use pointer arrays. Traversing the graph requires chasing pointers across non-contiguous memory, causing frequent CPU cache misses during every compiler phase.
4. **Expensive Late Scheduling (`Scheduler::ComputeSchedule`)**:
   Because pure nodes float without block assignments, turning a Sea-of-Nodes graph back into executable machine code requires running a full global scheduling pass (`ComputeScheduledGraph()`) that computes dominators, loop nesting, and earliest/latest legal block placements for every node. Scheduling alone consumes a significant fraction of total compile time.
5. **Path-Sensitive Analysis Is Difficult Without Block Order**:
   When nodes float outside basic blocks, it is difficult to maintain path-sensitive state (such as known object maps, eliminated branch conditions, or alias caches) during a forward walk, because there is no canonical basic block ordering until the graph is scheduled.

---

## 2. How Turboshaft's CFG Solves These Problems

Turboshaft (`src/compiler/turboshaft/`) replaces Sea of Nodes with an explicit CFG designed from the ground up for CPU cache locality, linear-time copying phases, and path-sensitive analysis.

### 1. Contiguous `OperationBuffer` Instead of Pointer-Chasing `Node*`
In Turboshaft (`src/compiler/turboshaft/graph.h`), operations are **not** individually allocated heap/zone objects connected by pointers:
* All operations across the entire `Graph` are packed sequentially into a single contiguous bump-pointer array of 64-bit `OperationStorageSlot`s called **`OperationBuffer`**.
* Every `Block` (`src/compiler/turboshaft/graph.h`) is simply a contiguous `[begin_, end_)` range of **`OpIndex`** byte offsets into the `OperationBuffer`.
* Iterating over the operations in a `Block` is a sequential scan over flat memory, maximizing hardware prefetching and L1/L2 cache hit rates.
* Side tables (`FixedOpIndexSidetable<T>`, `GrowingOpIndexSidetable<T>`) index directly by `OpIndex::id()` (`offset / 16`), replacing hash maps and node-pointer chasing with dense array lookups.

### 2. Dominance Invariant & Topological Block Ordering
Turboshaft enforces a strict structural invariant:
* **Topological / Dominator Order**: Every `Block` is assigned a `BlockIndex` in Reverse Post-Order (RPO), and operations are emitted into `OperationBuffer` in dominator-tree order (`GraphVisitor::VisitAllBlocks`).
* For any non-Phi operation at `OpIndex` `use`, every input `def` strictly dominates `use` and therefore has `def < use` in the `OperationBuffer` (with the sole exception of the backedge input of a loop header `PhiOp`, handled via `PendingLoopPhiOp`).
* Because operations are already scheduled inside basic blocks from the moment they are emitted, **Turboshaft needs no global `Scheduler` pass** before instruction selection.

### 3. Path-Sensitive Forward State via `SnapshotTable`
Because Turboshaft visits basic blocks in dominator-tree order (`src/compiler/turboshaft/copying-phase.h`), reducers can maintain rich path-sensitive state in a **`SnapshotTable`** (`src/compiler/turboshaft/snapshot-table.h`):
* As a reducer walks down the dominator tree, `SnapshotTable` records state updates in an append-only log (`O(1)` lookup and update).
* When switching between branches or merging predecessors at a join block, `SnapshotTable` rolls back to the Lowest Common Ancestor (LCA) in the dominator tree and replays/merges only the keys modified since that ancestor.
* This makes path-sensitive optimizations, such as **`LateLoadEliminationReducer`**, **`BranchEliminationReducer`**, **`ValueNumberingReducer` (GVN)**, and **`VariableReducer`**, both simpler and faster than Sea-of-Nodes effect-chain traversals.

### 4. Composable Out-of-Place Lowering (`CopyingPhase`)
In Sea of Nodes, lowering a single high-level node into a multi-block control-flow diamond (for example, lowering a checked Smi addition or an inlined fast allocation) requires mutating the graph in place, splitting basic blocks or control edges, and updating use lists.

In Turboshaft (`src/compiler/turboshaft/copying-phase.h`), phases transform an immutable `input_graph` into a fresh `output_graph` through a stack of composable template **`Reducers`**. Inside any `REDUCE(Op)` method, a reducer can emit arbitrary control flow (`IF (...) { ... } ELSE { ... }`, `GOTO`, `WHILE`) using the `Assembler`, and downstream reducers in the same phase immediately optimize the newly emitted blocks and operations in a single pass.

---

## 3. How Maglev's CFG Solves These Problems for Mid-Tier & Turbolev

Maglev (`src/maglev/`) is V8's mid-tier optimizing compiler and, via **Turbolev** (`src/compiler/turboshaft/turbolev-frontend-pipeline.cc`), the default frontend for Turboshaft. Maglev uses an explicit SSA CFG tailored for rapid bytecode-to-CFG construction and abstract interpretation:

* **Direct Bytecode-to-CFG Construction (`MaglevGraphBuilder`)**:
  Ignition bytecode is already a control-flow graph. Rather than converting bytecode into floating Sea-of-Nodes value/effect/control webs and then paying a scheduler to rediscover basic blocks, `MaglevGraphBuilder` translates bytecode basic blocks directly into Maglev `BasicBlock`s (`ZoneVector<Node*> nodes_` plus a terminating `ControlNode*`).
* **Implicit Effect Ordering by Block Sequence**:
  Because nodes in a Maglev `BasicBlock` execute sequentially in vector order, Maglev does not need explicit effect edges or `EffectPhi` nodes. Any side-effecting instruction (`NodeT::kProperties.can_write()`) simply updates the abstract interpreter state (`KnownNodeAspects::MarkPossibleSideEffect`) at that point in the linear walk.
* **Path-Sensitive Abstract Interpretation (`KnownNodeAspects`)**:
  As `MaglevGraphBuilder` (or `RecomputeKnownNodeAspectsProcessor` during post-inlining optimization) walks basic blocks in RPO order, it carries a **`KnownNodeAspects`** structure tracking each `ValueNode`'s refined `NodeType` bitmask, known `PossibleMaps`, cached property/context loads, and `AlternativeNodes` (cached `Int32`/`Float64`/`Tagged` representations). At control-flow joins, `KnownNodeAspects::Merge` intersects this knowledge in a single forward pass.
* **Zero-Schedule Handoff to Turboshaft (`TurbolevGraphBuildingPhase`)**:
  Because both Maglev and Turboshaft are SSA CFGs in RPO order, `TurbolevGraphBuildingPhase` (`src/compiler/turboshaft/turbolev-graph-builder.cc`) translates Maglev `BasicBlock`s and `Node`s directly into Turboshaft `Block`s and `OperationBuffer` entries in a single linear pass, completely eliminating the legacy Sea-of-Nodes frontend and scheduler.
