# Turboshaft Compiler Architecture

Turboshaft (`src/compiler/turboshaft/`) is V8's top-tier optimizing compiler architecture and backend IR. It replaces the legacy Sea-of-Nodes representation with a cache-friendly, typed Static Single Assignment (SSA) Control-Flow Graph (CFG) and a template-based copying reducer framework.

Turboshaft is **mandatory** for all top-tier compilation in V8 (`--no-turboshaft` implies `--no-turbofan`, and `InstructionSelector` operates exclusively on Turboshaft graphs).

---

## 1. Conceptual Overview: End-to-End Turboshaft Pipeline

A top-tier compilation enters Turboshaft through one of two frontends in `src/compiler/pipeline.cc` and then executes a series of Turboshaft `CopyingPhase` passes followed by instruction selection and code generation:

```
                  [ Ignition Bytecode + FeedbackVector ]
                                    |
          +-------------------------+-------------------------+
          |                                                   |
   Default: Turbolev (--turbolev)              Legacy SoN (--no-turbolev)
   ==============================              ==========================
   1. TurbolevFrontendPipeline                 1. BytecodeGraphBuilder & SoN
      (builds & optimizes maglev::Graph)          Inlining / Typer / Lowering
   2. TurbolevGraphBuildingPhase               2. ComputeScheduledGraph()
      (translates Maglev CFG ->                   + BuildGraphPhase
       Turboshaft CFG directly)                   (Schedule -> Turboshaft CFG)
          |                                                   |
          +-------------------------+-------------------------+
                                    |
                                    v
              [ Turboshaft Optimization Phases (CFG) ]
              ========================================
              - MachineLoweringPhase / FastApiCallLowering
              - LoopPeelingPhase / LoopUnrollingPhase (if not Turbolev)
              - StoreStoreEliminationPhase ( + LateLoadElimination )
              - OptimizePhase (StructuralOptimization, BranchElimination,
                MachineOptimization, ValueNumbering)
              - TypedOptimizationsPhase
              - DecompressionOptimizationPhase (in-place)
                                    |
                                    v
              [ Turboshaft Backend & Code Generation ]
              ========================================
              - SpecialRPO (TurboshaftSpecialRPONumberer)
              - PropagateDeferred & ProfileApplicationPhase
              - InstructionSelectionPhase (InstructionSelector::SelectInstructions)
              - Register Allocation, Frame Elision, Jump Threading, Code Assembly
```

---

## 2. The Turbolev Frontend (`TurbolevFrontendPipeline`)

When `--turbolev` is enabled (`PipelineImpl::CreateGraphWithMaglev()` in `src/compiler/pipeline.cc`), V8 bypasses the legacy Sea-of-Nodes graph and scheduler entirely. Instead, **`TurbolevFrontendPipeline::Run()`** (`src/compiler/turboshaft/turbolev-frontend-pipeline.cc`) constructs and optimizes a `maglev::Graph` using a dedicated sequence of Maglev phases before translating it into a Turboshaft `Graph`:

### Phase-by-Phase Execution in `TurbolevFrontendPipeline::Run()`

1. **`MaglevGraphBuilderPhase` (`MaglevGraphBuilding`)**:
   Runs `maglev::MaglevGraphBuilder::Build()` (`src/maglev/maglev-graph-builder.cc`) on the top-level compilation unit. During bytecode traversal, small callees that meet the eager inlining budget are inlined inline on the fly, while larger candidates are recorded as `MaglevCallSiteInfo` entries in `graph->inlineable_calls()`.
2. **`InlinerPhase` (`Inlining`, when `--turbolev-non-eager-inlining`)**:
   Runs `maglev::MaglevInliner::Run()` (`src/maglev/maglev-inlining.cc`), which greedily pops the highest-priority `MaglevCallSiteInfo` from the max-heap, builds its subgraph, splices it into the caller CFG, and after each inlining batch runs a combined `GraphMultiProcessor` pass (`MaglevGraphOptimizer`, `ReachableExceptionHandlerTracker`, and `RecomputeKnownNodeAspectsProcessor`) plus `RemoveUnreachableBlocks()`.
3. **`LoopPeelerPhase` (`LoopPeeling`, when `--turbolev-non-eager-loop-peeling`)**:
   Runs `maglev::MaglevLoopPeeler::Run()` (`src/maglev/maglev-loop-peeler.cc`) to peel hot loops. If at least one loop was peeled (`peeled == true`), it immediately runs **`PostOptimizerPhase`** (`MaglevGraphOptimizer` + `CommonSubexpressionEliminationProcessor` + `ReachableExceptionHandlerTracker` + `RecomputeKnownNodeAspectsProcessor`) to fold redundant checks and loads exposed in the peeled body.
4. **`TruncationPhase` (`Truncation`, when `--maglev-truncation` and `graph->may_have_truncation()`)**:
   Runs a two-pass truncation optimization (`src/maglev/maglev-truncation.h`):
   * Pass 1 (`GraphBackwardProcessor<PropagateTruncationProcessor>`): Propagates `CanTruncateToInt32` flags backward from bitwise/truncating uses while verifying via `GetMaxExactValue` that intermediate `Float64Add`/`Subtract` sums cannot exceed `kMaxExactlyRepresentableValue` ($2^{53}$).
   * Pass 2 (`GraphProcessor<TruncationProcessor>`): Rewrites truncatable `Float64Add`/`Subtract`/`Multiply`/`Divide`, `Float64SpeculateSafeAdd`, and `Int32AddWithOverflow`/`SubtractWithOverflow`/`MultiplyWithOverflow` nodes into non-overflow-checking wrapping `Int32` operations.
5. **`PrePhiUntaggingPhase` and `PhiUntaggingPhase` (when `--turbolev-untagged-phis`)**:
   * `PrePhiUntaggingPhase`: Runs a `GraphMultiProcessor` combining `MaglevGraphOptimizer`, `ReachableExceptionHandlerTracker`, `RecomputeKnownNodeAspectsProcessor`, `RecomputePhiUseHintsProcessor` (which refreshes `use_repr_hints` and `same_loop_use_repr_hints` on all `Phi`s after inlining/peeling/truncation, and pins `Phi`s referenced by deopt frames with `UseRepresentation::kNonTruncated`), and `BoundsCheckEliminationProcessor`.
   * `PhiUntaggingPhase`: Runs `ReachableExceptionHandlerTracker` and `MaglevPhiRepresentationSelector` (`src/maglev/maglev-phi-representation-selector.h`) to convert `Tagged` phis to `kInt32`, `kUint32`, `kFloat64`, or `kHoleyFloat64` and bypass/replace obsolete tagging and untagging conversions.
6. **`RangeAnalysisPhase` + `PostOptimizerPhase` (when `--maglev-range-analysis`)**:
   Runs `maglev::NodeRanges::ProcessGraph()` (`src/maglev/maglev-range-analysis.h`) to compute static integer range intervals on the CFG, followed by **`PostOptimizerPhase`** (`MaglevGraphOptimizer` with `&ranges` + `CommonSubexpressionEliminationProcessor` + `ReachableExceptionHandlerTracker` + `RecomputeKnownNodeAspectsProcessor`) to eliminate overflow checks and comparisons proven redundant by range analysis.
7. **`EscapeAnalysisPhase` + `PostOptimizerPhase` (when `--turbolev-escape-analysis`)**:
   Runs `maglev::EscapeAnalysis::Run(graph, compilation_info, temp_zone)` (`src/maglev/turbolev-escape-analysis.h`) to scalar-replace captured `InlinedAllocation` objects and synthesize `Phi` nodes for their fields at control merges, followed by another **`PostOptimizerPhase`** to fold newly exposed constants and field values.
8. **`PostHocPhase` (`AnyUseMarking`)**:
   If `!v8_flags.turbolev_escape_analysis`, calls `graph->UnwrapDeoptFrames()` to strip `Identity` wrappers from all deopt frames (when escape analysis is enabled, it already unwraps deopt frames). Then runs `ReturnedValueRepresentationSelector` and `AnyUseMarkingProcessor` to compute transitive `use_count`s across nodes and deopt frames.
9. **`DeadNodeSweepingPhase` (`DeadNodeSweeping`)**:
   Runs `GraphProcessor<DeadNodeSweepingProcessor>` to remove unused pure nodes (`!is_required_when_unused() && !is_used()`) and unused `Phi`s left behind by earlier passes and escape analysis.

After `TurbolevFrontendPipeline::Run()` returns the optimized `maglev::Graph*`, `PipelineImpl::CreateGraphWithMaglev()` invokes **`TurbolevGraphBuildingPhase`** (`src/compiler/turboshaft/turbolev-graph-builder.cc`), which walks the Maglev CFG and emits the corresponding Turboshaft operations into `PipelineData::graph()`.

---

## 3. Core Turboshaft Optimization Phases

Once the Turboshaft `Graph` is populated (either via `TurbolevGraphBuildingPhase` or legacy `BuildGraphPhase`), `PipelineImpl::OptimizeTurboshaftGraph()` runs a sequence of `CopyingPhase<Reducers...>` passes (`src/compiler/turboshaft/`):

* **`MachineLoweringPhase`**: Lowers high-level `Simplified` operations (`CheckMaps`, `ConvertJSPrimitiveToUntagged`, `StringAt`, `TransitionAndStoreArrayElement`, `Allocate`, etc.) into low-level `Machine` operations (`Load`, `Store`, `WordBinop`, `DeoptimizeIf`, `Branch`) using `JSGenericLoweringReducer`, `DataViewLoweringReducer`, `MachineLoweringReducer`, `FastApiCallLoweringReducer`, `SelectLoweringReducer`, and `MachineOptimizationReducer`.
* **`LoopUnrollingPhase` / `LoopPeelingPhase`**: Analyzes loop headers and clones loop subgraphs via `GraphVisitor::CloneSubGraph`.
* **`StoreStoreEliminationPhase`**: Runs `StoreStoreEliminationReducer` (`RedundantStoreAnalysis` in `src/compiler/turboshaft/store-store-elimination-reducer-inl.h`), `LateLoadEliminationReducer`, `MachineOptimizationReducer`, `BranchEliminationReducer`, and `ValueNumberingReducer` to eliminate redundant heap stores and loads using path-sensitive alias tracking in `SnapshotTable`:
  - Walking blocks backward, `RedundantStoreAnalysis::ProcessBlock` tracks field writes as `kUnobservable` until an operation with `effects.requires_consistent_heap()` transitions all tracked stores to **`StoreObservability::kGCObservable`** (`MarkAllStoresAsGCObservable()`). When a `StoreOp` is encountered in the `kGCObservable` state, if `store.maybe_initializing_or_transitioning` is true, it **cannot** be eliminated across a GC safepoint; instead, `RedundantStoreAnalysis` keeps the store and calls `table_.MarkStoreAsUnobservable(store.base(), store.offset, size)` so any earlier stores to the same field before the safepoint can be eliminated, whereas non-initializing stores (`!store.maybe_initializing_or_transitioning`) are added to `eliminable_stores_`.
  - When finishing `ProcessBlock` for a loop header (`block.IsLoop()` in `RedundantStoreAnalysis::Run`), `table_.InvalidateBasesInRange(block_index, back_edge->index())` resets stores to objects allocated inside the loop (`[block_index, back_edge->index()]`) back to `kObservable` before `table_.Seal(&needs_revisit)`, preventing stores in a later loop iteration from eliminating stores to a previous iteration's allocation.
* **`OptimizePhase`**: Runs `StructuralOptimizationReducer` (switch folding, branch-to-select), `LateEscapeAnalysisReducer`, `PretenuringPropagationReducer`, `MemoryOptimizationReducer` (allocation folding and write-barrier elimination), `MachineOptimizationReducer`, and `ValueNumberingReducer`.
* **`DecompressionOptimizationPhase`**: An in-place analysis pass (`src/compiler/turboshaft/decompression-optimization.cc`) that walks operations backward to avoid unnecessary 32-to-64-bit pointer decompressions when only the lower 32 compressed bits are needed.

---

## 4. Instruction Selection on Turboshaft (`InstructionSelectionPhase`)

Instruction selection (`src/compiler/turboshaft/instruction-selection-phase.cc` and `src/compiler/backend/instruction-selector.cc`) translates the Turboshaft `Graph` into a backend `InstructionSequence` of target-specific `Instruction`s (`ArchOpcode` + `InstructionOperand`s).

### 1. Special Reverse Post-Order (`TurboshaftSpecialRPONumberer`)
Before selecting instructions, `InstructionSelectionPhase::Run` runs **`TurboshaftSpecialRPONumberer::ComputeSpecialRPO()`** (`src/compiler/turboshaft/instruction-selection-phase.cc`):
* Performs an iterative depth-first traversal of the Turboshaft `Graph` to detect loop backedges (`rpo_number(succ) == kBlockOnStack`).
* If loops exist, `ComputeLoopInfo` propagates loop membership backward from each backedge to its loop header using `SparseBitVector`s.
* Performs a second post-order traversal that delays outgoing edges leaving a loop until every block belonging to the loop body (and any nested inner loops) has been visited. This guarantees that all blocks of a loop occupy a **contiguous range of RPO numbers**, which is required by the backend register allocator's loop live-range extension.
* Next, `PropagateDeferred(graph)` propagates deferred-block hints (`Block::CustomDataKind::kDeferredInSchedule`) along single-predecessor edges from `BranchHint`s so cold blocks are placed out-of-line at the end of the function.

### 2. Bottom-Up Visitation & `effect_level_` Tracking
`InstructionSelector::SelectInstructions()` (`src/compiler/backend/instruction-selector.cc`) first marks all `PhiOp` inputs in loop headers as `Used`, and then visits basic blocks in **reverse RPO order (bottom-up)** (`base::Reversed(blocks)`). Within each `Block`, `VisitBlock` scans operations from bottom to top, while pre-computing a per-operation **`effect_level_`** counter in a forward scan:
* Whenever an operation has `Effects().can_write()` (or produces control flow / cannot be reordered across loads), `effect_level` is incremented.
* If an operation `op` is a pure read (`op.Effects().can_read() && !op.Effects().can_write()`), its recorded `effect_level_[op]` is bumped by `+1` relative to preceding non-writing ops when followed by a write, capturing the exact state epoch in which the load executes.

### 3. Tree Pattern Matching & `InstructionSelector::CanCover(user, node)`
Backend instruction selectors (x64, arm64, ia32, arm) frequently fold a producer `node` into a consumer `user` to emit a single complex machine instruction (for example, folding a shift and an add into an x64 LEA or ARM64 shifted-register instruction, or folding a `Load` or `Comparison` into a `Branch`).

Folding `node` into `user` means `node` will be evaluated at `user`'s position rather than at `node`'s original position. **`InstructionSelector::CanCover(OpIndex user, OpIndex node)`** (`src/compiler/backend/instruction-selector.cc`) enforces four safety conditions before allowing this fold:
1. **Same Basic Block (`InCurrentBlock(node)`)**: Both `user` and `node` must belong to `current_block_`.
2. **Pure No-Produce Fast Path**: If `op.Effects().produces.bits() == 0` (the operation produces no side effects or state dependencies that later operations depend on), it skips the effect-level check and only checks `is_exclusive_user_of(schedule(), user, node)`.
3. **Matching Effect Level (`GetEffectLevel(node) == current_effect_level_`)**: If `node` produces effects or reads mutable state, its `effect_level_` must equal `user`'s `current_effect_level_`, proving that no interfering store or call occurred between `node` and `user`.
4. **Exclusive Use (`is_exclusive_user_of(schedule(), user, node)`)**: Checks `value_op.saturated_use_count`: all uses of `node` in the entire graph must belong to `user` (or, in the implicit-truncation special case `value -> TruncateWord64ToWord32 -> user`, both `value` and the intermediate truncation must have `saturated_use_count.Is(1)`). This guarantees that folding `node` into `user` will not duplicate computation for another consumer.

---

## 5. Further Reading in `docs/compiler/turboshaft/`

* [Turboshaft IR (`compiler-turboshaft-ir.md`)](./compiler-turboshaft-ir.md): Detailed reference on `OperationBuffer`, `OpIndex` (`generation_mod2`), `V<T>` / `OptionalV<T>` / `ConstOrV<T>`, `Explode`, and `THROWING_OP_BOILERPLATE`.
* [Turboshaft Reducers (`compiler-turboshaft-reducers.md`)](./compiler-turboshaft-reducers.md): Detailed reference on `Assembler`, `TURBOSHAFT_REDUCER_BOILERPLATE`, `REDUCE` vs. `REDUCE_INPUT_GRAPH`, and control-flow macros (`IF`/`ELSE`, `GOTO`, `BIND_LOOP`).
* [Turboshaft Copying Approach (`compiler-turboshaft-copying-approach.md`)](./compiler-turboshaft-copying-approach.md): Detailed reference on `GraphVisitor`, why `MayThrow` ops return `OpIndex::Invalid()` in `VisitOpNoMappingUpdate`, `CheckExceptionOp`/`DidntThrowOp`, `SnapshotTable`, `VariableReducer`, and `PendingLoopPhiOp` -> `FixLoopPhis`.
