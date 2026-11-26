# Maglev Compiler Architecture (`src/maglev/`)

Maglev is V8's mid-tier optimizing JIT compiler, positioned between the Sparkplug baseline compiler and the top-tier Turboshaft compiler. In addition to generating fast mid-tier machine code directly, Maglev's graph builder and optimization passes serve as the default **Turbolev** frontend for Turboshaft (`src/compiler/turboshaft/turbolev-frontend-pipeline.cc`).

Maglev achieves compilation speeds roughly an order of magnitude faster than legacy TurboFan while delivering substantial speedups over Sparkplug by combining:
1. **Direct Bytecode-to-SSA-CFG Construction**: Translating Ignition bytecode basic blocks directly into an explicit Static Single Assignment (SSA) Control-Flow Graph (`Graph` of `BasicBlock`s).
2. **Forward Abstract Interpretation (`KnownNodeAspects`)**: Tracking dynamic type refinements (`NodeType`), known `Map` sets (`PossibleMaps`), representation alternatives (`AlternativeNodes`), and available loads/expressions in a single forward walk without Sea-of-Nodes effect chains.
3. **Targeted Linear-Time Optimization Passes**: Running composable `GraphProcessor` passes over the CFG before a single-pass forward register allocator (`StraightForwardRegisterAllocator`) and direct `MacroAssembler` code generation.

---

## 1. Complete `MaglevCompiler::Compile` Pipeline

The complete mid-tier compilation pipeline is orchestrated by **`MaglevCompiler::Compile(LocalIsolate*, MaglevCompilationInfo*)`** in `src/maglev/maglev-compiler.cc`:

```
  [ Ignition Bytecode + FeedbackVector ]
                    |
                    v
  1. MaglevGraphBuilder::Build()
     (Bytecode -> SSA CFG,KnownNodeAspects, eager small-function inlining,
      peeled loops, records inlineable_calls())
                    |
                    v
  2. MaglevInliner::Run() (if --maglev-non-eager-inlining)
     (Greedily inlines call sites from priority queue; after each batch runs:
      MaglevGraphOptimizer + ReachableExceptionHandlerTracker +
      RecomputeKnownNodeAspectsProcessor + RemoveUnreachableBlocks(),
      then graph_->UnwrapDeoptFrames() at the end of Run())
                    |
                    v
  3. Two-Pass Truncation (if --maglev-truncation && may_have_truncation())
     - GraphBackwardProcessor<PropagateTruncationProcessor>
     - GraphProcessor<TruncationProcessor>
                    |
                    v
  4. Phi Untagging (if --maglev-untagged-phis)
     - GraphMultiProcessor<ReachableExceptionHandlerTracker&,
       MaglevPhiRepresentationSelector&>
                    |
                    v
  5. Deopt Frame Unwrapping & Use Marking
     - graph->UnwrapDeoptFrames()
     - ReturnedValueRepresentationSelector
     - AnyUseMarkingProcessor + RegallocNodeInfoAllocationProcessor
                    |
                    v
  6. Pre-Regalloc Processors (5 combined passes)
     - DeadNodeSweepingProcessor
     - ValueLocationConstraintProcessor
     - MaxCallDepthProcessor
     - LiveRangeAndNextUseProcessor
     - DecompressedUseMarkingProcessor
                    |
                    v
  7. StraightForwardRegisterAllocator
     + kMaxStackSlots check (tagged_stack_slots + untagged_stack_slots)
                    |
                    v
  8. MaglevCodeGenerator::Assemble() (on background thread)
```

### Detailed Stage Breakdown (`src/maglev/maglev-compiler.cc`)

1. **Graph Building (`MaglevGraphBuilder::Build()`)**:
   * Runs `PrewalkBytecode()` / `CalculatePredecessorCounts()` using `compiler::BytecodeAnalysis` to identify merge points, loop headers, and liveness.
   * Walks bytecodes in RPO order, maintaining `InterpreterFrameState current_interpreter_frame_` and `KnownNodeAspects`.
   * **Eager vs. Non-Eager Inlining**: When visiting a monomorphic `Call` bytecode (`TryBuildInlineFunction`), small callees whose bytecode length fits within `--max-maglev-inlined-bytecode-size-small` are **inlined eagerly inline** during `MaglevGraphBuilder::Build()`. Larger inlining candidates are recorded as **`MaglevCallSiteInfo`** entries (carrying a snapshot of `MaglevCallerDetails` and a priority `score`) in `graph->inlineable_calls()`.
2. **Non-Eager Inlining (`MaglevInliner::Run()` in `src/maglev/maglev-inlining.cc`)**:
   * While `graph->inlineable_calls()` is non-empty and cumulative inlined bytecode budget remains, pops the highest-scoring `MaglevCallSiteInfo` and runs `BuildInlineFunction` to replace the generic `CallKnownJSFunction` node with the callee's CFG subgraph.
   * After each inlining round (unless `--maglev-optimize-inlined-calls-once` defers it to the end), `MaglevInliner::RunOptimizer()` runs a **`GraphMultiProcessor`** combining:
     * **`MaglevGraphOptimizer`**: Uses `MaglevReducer` and updated node types/constants to fold checks, arithmetic, and loads exposed by inlining.
     * **`ReachableExceptionHandlerTracker`**: Tracks which `catch` handler blocks can still be reached by throwing nodes.
     * **`RecomputeKnownNodeAspectsProcessor`**: Recomputes `KnownNodeAspects` across the merged caller+callee CFG.
   * Calls `graph_->RemoveUnreachableBlocks()` if constant-folded branches made blocks dead, and calls `graph_->UnwrapDeoptFrames()` at the end of `MaglevInliner::Run()`.
3. **Two-Pass Float64/Int32 Truncation (`src/maglev/maglev-truncation.h`)**:
   When `--maglev-truncation` is enabled and the graph contains bitwise or truncating operations (`graph->may_have_truncation()`):
   * **Pass 1 (`GraphBackwardProcessor<PropagateTruncationProcessor>`)**: Walks the graph backward. Every `ValueNode` starts with `can_truncate_to_int32() == true`. Non-truncating consumers (including `eager_deopt_info` and `lazy_deopt_info` frames) clear `can_truncate_to_int32(false)` on their inputs. For additive chains (`Float64Add`, `Float64Subtract`, `Float64SpeculateSafeAdd`, `Int32AddWithOverflow`, `Int32SubtractWithOverflow`), `GetMaxExactValue(node)` sums static input ranges to verify that no intermediate sum can exceed **`kMaxExactlyRepresentableValue`** ($2^{53}$, where IEEE-754 rounding diverges from modular 32-bit addition).
   * **Pass 2 (`GraphProcessor<TruncationProcessor>`)**: Walks the graph forward, replacing `Float64` binary operations (where `can_truncate_to_int32()` is true and inputs are `Int32`-convertible), `Float64SpeculateSafeAdd`, and `Int32*WithOverflow` nodes with fast wrapping `Int32Add` / `Int32Subtract` / `Int32Multiply` nodes.
4. **Phi Representation Selection (`MaglevPhiRepresentationSelector` in `src/maglev/maglev-phi-representation-selector.h`)**:
   * (In the Turbolev frontend pipeline, `TurbolevFrontendPipeline::PrePhiUntaggingPhase` first runs `RecomputePhiUseHintsProcessor` alongside `MaglevGraphOptimizer` and `RecomputeKnownNodeAspectsProcessor` before phi untagging.)
   * By default, `MaglevGraphBuilder` creates all `Phi` nodes with `ValueRepresentation::kTagged`.
   * In `PreProcessBasicBlock`, `MaglevPhiRepresentationSelector::ProcessPhi` inspects each `Phi`'s incoming inputs (`UntaggingKind`: `kSmiConstant`, `kHeapNumberConstant`, `kUndefinedConstant`, `kConversion`, `kUntaggedPhi`, `kKnownSmi`, `kKnownNumber`, `kSelfBackedge`, `kSpeculativePhiBackedge`, `kSpeculativeAny`, `kSpeculativeOSRValue`) and its recorded `use_repr_hints()`.
   * When profitable, `ConvertTaggedPhiTo(phi, repr, ...)` converts the `Phi` in-place to an untagged representation (**`kInt32`**, **`kUint32`**, **`kFloat64`**, or **`kHoleyFloat64`**) and unboxes its incoming inputs in predecessor blocks.
   * For downstream nodes that previously untagged the `Phi` (e.g., `CheckedSmiUntag(phi)`), `UpdateUntaggingOfPhi` overwrites the now-redundant conversion node in-place with **`Identity`** (`OverwriteWithIdentityTo(phi)`), or replaces it with an untagged-to-untagged conversion (e.g., `ChangeInt32ToFloat64`).
   * Conversely, if a consumer still needs a `Tagged` value from an untagged `Phi` (or in a deopt frame), `EnsurePhiTagged` inserts a re-tagging node (`Int32ToNumber`, `CheckedSmiTagInt32`, `Float64ToTagged`, `HoleyFloat64ToTagged`) and caches it in a `turboshaft::SnapshotTable<ValueNode*> phi_taggings_` keyed by the `Phi` so dominated blocks reuse the same boxed value.
5. **Deopt Frame Unwrapping, Returned Value Selection, and Use Marking**:
   * **`graph->UnwrapDeoptFrames()`**: Walks all deduplicated `eager_deopt_top_frames()` and `lazy_deopt_top_frames()` and replaces any `Identity` node references (created by `MaglevPhiRepresentationSelector` or `MaglevGraphOptimizer`) with their underlying unwrapped `ValueNode*`.
   * **`ReturnedValueRepresentationSelector`**: Unwraps `ReturnedValue` nodes. (Note: `maglev::EscapeAnalysis::Run` in `src/maglev/turbolev-escape-analysis.cc`, gated by `--turbolev-escape-analysis`, runs only in `TurbolevFrontendPipeline`, not in standalone `MaglevCompiler::Compile`.)
   * **`AnyUseMarkingProcessor`** and **`RegallocNodeInfoAllocationProcessor`**: Marks live nodes by walking inputs and deopt frames backward, drops dead `AllocationBlock` instructions, and allocates `RegallocValueNodeInfo` / `RegallocNodeInfo` on every remaining node.
6. **The 5 Pre-Regalloc Processors (`src/maglev/maglev-pre-regalloc-codegen-processors.h`)**:
   Combined into a single `GraphMultiProcessor` pass:
   * **`DeadNodeSweepingProcessor`**: Deletes nodes with `!is_required_when_unused() && !is_used()`.
   * **`ValueLocationConstraintProcessor`**: Calls `node->SetValueLocationConstraints()` on every node to configure `UnallocatedOperand` policies for inputs, outputs, and temporaries.
   * **`MaxCallDepthProcessor`**: Computes `max_call_stack_args_` (adding `kAllocatableGeneralRegisterCount + kAllocatableDoubleRegisterCount` for nodes with `needs_register_snapshot()`) and `max_deopted_stack_size_` across all deopt frames for stack check sizing.
   * **`LiveRangeAndNextUseProcessor`**: Assigns sequential `NodeIdT` IDs, builds `InputLocation` next-use chains, extends live ranges of outer values across loop backedges (`JumpLoop::set_used_nodes`), and computes loop `reload_hints_` / `spill_hints_`.
   * **`DecompressedUseMarkingProcessor`**: Under pointer compression (`V8_COMPRESS_POINTERS`), calls `node->MarkTaggedInputsAsDecompressing()`.
7. **Register Allocation & `kMaxStackSlots` Bailout**:
   Runs **`StraightForwardRegisterAllocator`** (see [Maglev Register Allocator (`compiler-maglev-regalloc.md`)](./compiler-maglev-regalloc.md)). Afterwards, `MaglevCompiler::Compile` checks:
   ```cpp
   int stack_slots = graph->tagged_stack_slots() + graph->untagged_stack_slots();
   if (stack_slots > kMaxStackSlots) {
     compilation_info->set_could_not_inline_all_candidates();
     return false;  // Bails out of compilation cleanly.
   }
   ```
   Here `constexpr uint32_t kMaxStackSlots = 4 * KB / kSystemPointerSize` (512 slots on 64-bit). Because the function prologue grows the frame with a single unchecked stack pointer subtraction before running `FunctionEntryStackCheck`, frames larger than the smallest OS page size (4 KB) must bail out (`set_could_not_inline_all_candidates()`) so they never step over the stack guard page.

---

## 2. `KnownNodeAspects`, `NodeInfo`, and the `NodeType` Lattice

During graph building (`MaglevGraphBuilder`) and post-inlining optimization (`RecomputeKnownNodeAspectsProcessor`), Maglev tracks path-sensitive facts about live `ValueNode*`s in **`KnownNodeAspects`** (`src/maglev/maglev-known-node-aspects.h`).

### 1. The 18-Bit `NodeType` Bitmask Lattice (`src/maglev/maglev-node-type.h`)
Every `ValueNode` has a static type (`node->GetStaticType(broker)`) and an optional refined dynamic type stored in `NodeInfo::type()`. `NodeType` is implemented as an **18-bit leaf bitmask** (`LEAF_NODE_TYPE_LIST`) where every concrete heap/value category is a single bit and composite types (`COMBINED_NODE_TYPE_LIST`) are bitwise combinations:

* **18 Leaf Bits (`LEAF_NODE_TYPE_LIST`, bits 0..17)**:
  * `kSmi` (`1 << 0`), `kHeapNumber` (`1 << 1`), `kNull` (`1 << 2`), `kUndefined` (`1 << 3`), `kBoolean` (`1 << 4`), `kSymbol` (`1 << 5`)
  * Strings: `kInternalizedString` (`1 << 6`), `kOtherString` (`1 << 7`)
  * `kContext` (`1 << 8`)
  * Receivers and heap objects: `kStringWrapper` (`1 << 9`), `kJSArray` (`1 << 10`), `kJSFunction` (`1 << 11`), `kOtherCallable` (`1 << 12`), `kJSDataView` (`1 << 13`), `kJSGeneratorObject` (`1 << 14`), `kOtherHeapObject` (`1 << 15`), `kOtherJSReceiver` (`1 << 16`)
  * `kBigInt` (`1 << 17`)
* **Combined Bitmasks (`COMBINED_NODE_TYPE_LIST`)**:
  * `kUnknown = (1 << kNumberOfLeafNodeTypes) - 1` (all 18 leaf bits set)
  * `kNone = 0` (contradiction / unreachable code)
  * `kCallable = kJSFunction | kOtherCallable`
  * `kNullOrUndefined = kNull | kUndefined`
  * `kOddball = kNullOrUndefined | kBoolean`
  * `kNumber = kSmi | kHeapNumber`
  * `kNumeric = kNumber | kBigInt`
  * `kString = kInternalizedString | kOtherString`
  * `kName = kString | kSymbol`
  * `kJSPrimitive = kNumeric | kName | kOddball`
  * `kJSReceiver = kJSArray | kCallable | kStringWrapper | kJSDataView | kJSGeneratorObject | kOtherJSReceiver`
  * `kAnyHeapObject = kUnknown - kSmi`
* **O(1) Lattice Operations**:
  * `IntersectType(left, right)`: Bitwise AND (`left & right`).
  * `UnionType(left, right)`: Bitwise OR (`left | right`).
  * `NodeTypeIs(sub, super)`: Subset check (`(sub & ~super) == 0` and `sub != kNone`).
  * When `KnownNodeAspects::EnsureType(broker, node, required_type)` intersects a node's current type with `required_type` and produces `NodeType::kNone`, it returns `EnsureTypeResult::kContradiction`, allowing the graph builder or optimizer to immediately emit an unconditional deopt or mark the branch dead.

### 2. `NodeInfo`: `PossibleMaps` and `AlternativeNodes`
For each tracked `ValueNode*`, `KnownNodeAspects::node_infos_` stores a **`NodeInfo`** (`src/maglev/maglev-known-node-aspects.h`) containing:
1. **`NodeType type_`**: The refined dynamic type bitmask.
2. **`PossibleMaps possible_maps_`** (`compiler::ZoneRefSet<Map>`):
   * Tracks the exact set of `MapRef`s an object can have at the current program point (`possible_maps_are_known_`).
   * Tracks `any_map_is_unstable_`: If all known maps in `possible_maps_` are stable (`map.is_stable()`), Maglev records a compilation dependency (`broker->dependencies()->DependOnStableMap(map)` via `KnownMapsMerger::UpdateKnownNodeAspects`) instead of flushing the map knowledge on side-effecting calls!
   * Tracks `maps_are_stale_`: If a side effect might have transitioned an unstable map, `maps_are_stale_` is set to `true` (so subsequent `CheckMaps` must re-verify the map, while `BuildCheckMaps` still knows what the previous map was).
3. **`AlternativeNodes alternative_`**:
   * Stores 6 slots defined by the `ALTERNATIVES` macro (`tagged`, `int32`, `truncated_int32_to_number`, `float64`, `holey_float64`, and `checked_value`) in a compact `std::array<ValueNode*, Kind::kNumberOfAlternatives>`.
   * Accessed via typed methods (`tagged()` / `set_tagged()` / `get_or_set_tagged()`, `int32()` / `set_int32()`, `truncated_int32_to_number()`, `float64()` / `set_float64()`, `holey_float64()` / `set_holey_float64()`, `checked_value()` / `set_checked_value()`, and `get(UseRepresentation)`).
   * Whenever Maglev converts a `ValueNode*` `v` (for example, via builder helpers like `GetInt32Element`, `GetFloat64Element`, or `GetTaggedValue`, or during `RecomputeKnownNodeAspectsProcessor`), it records the converted node on `v`'s canonical `NodeInfo` (`KnownNodeAspects::GetCanonical(v)`): untagged-to-untagged conversions, `Identity`, and `ReturnedValue` unwrap to the underlying non-conversion node so conversion chains share a single `NodeInfo`, while `Tagged <-> Untagged` boundaries keep distinct `NodeInfo` keys so type/map refinements on a `Tagged` output are not unsoundly propagated back to a `Tagged` input (e.g. a Smi-ranged `HeapNumber`). Tagging conversions are recorded unidirectionally in `input_info->alternative().set_tagged(tagged)` without allocating a second `NodeInfo` entry for the tagging conversion node itself. Any subsequent operation in the same basic block (or dominated blocks) queries the corresponding `AlternativeNodes` accessor (or unwraps a tagging conversion input directly) to reuse the existing representation and avoid duplicate tagging and untagging instructions.

### 3. Property/Context Load Caches & `AvailableExpressions` (CSE)
In addition to `node_infos_`, `KnownNodeAspects` maintains five path-sensitive caches:
* `loaded_constant_properties_` (`PropertyKey -> ValueNode* -> ValueNode*`): Cached constant property loads (protected by `CompilationDependencies`; **survives across side-effecting calls**).
* `loaded_properties_`: Cached mutable property loads (`PropertyKey::Elements()`, `length`, or named in-object/backing-store fields). Cleared selectively by field/element stores or globally by arbitrary side effects.
* `loaded_tagged_keyed_properties_`: Bounded cache (`kMaxTaggedKeyedProperties = 100`) mapping `(elements, index)` to `LoadFixedArrayElement*`.
* `loaded_context_constants_` and `loaded_context_slots_`: Cached `Context` slot reads, with alias tracking (`ContextSlotLoadsAlias`: `kNever`, `kOnlyLoadsRelativeToCurrentContext`, `kOnlyLoadsRelativeToConstant`, `kAlways`).
* `available_expressions_`: Hash table mapping `(hash -> {NodeBase*, effect_epoch})` for on-the-fly Common Subexpression Elimination (CSE) (`FindExpression` / `AddExpression`). Pure instructions use `kEffectEpochForPureInstructions` (`UINT32_MAX`), while effect-sensitive instructions check `effect_epoch_ <= it->second.effect_epoch`.

### 4. Side-Effect Invalidation (`MarkPossibleSideEffect`) & `LoopEffects`
Whenever a node `node` is added to the graph, `KnownNodeAspects::MarkPossibleSideEffect(node, broker, ...)` checks `NodeT::kProperties.can_write()`:
* If `!can_write()`, nothing is invalidated.
* If `can_write()` is true:
  1. Increments `effect_epoch_` (invalidating non-pure CSE entries).
  2. Clears `loaded_tagged_keyed_properties_` unless `PreservesTaggedKeyedProperties(opcode)` holds.
  3. Performs **surgical invalidation** for known writes:
     * `Builtin::kCloneFastJSArray` does not invalidate KNA (`BuiltinInvalidatesKNA` returns `false`).
     * `MaybeGrowFastElements` only clears `loaded_properties_` for `broker->length_string()`.
     * `IsElementsArrayWrite(opcode)` only clears `PropertyKey::Elements()`.
     * `CheckMapsWithMigration` / `MigrateMapIfNeeded`, `StoreMap`, and `TransitionElementsKind` only invalidate unstable maps matching the migrated/stored/transitioned targets (`ClearUnstableNodeAspectsFor...`).
     * Simple field stores (`IsSimpleFieldStore`) and TypedArray stores (`IsTypedArrayStore`) update their specific property key directly when emitted and do **not** clear other unstable node aspects.
     * All other writes (arbitrary JS calls, runtime calls, generic stores) call **`ClearUnstableNodeAspects()`**, which clears `loaded_properties_`, `loaded_context_slots_`, and marks any unstable `PossibleMaps` as stale (skipping the `node_infos_` scan entirely if `!side_effects_require_invalidation_`).

#### Loop Header Merging & `LoopEffects`
When entering a loop header, `MaglevGraphBuilder` has not yet visited the loop body, so it cannot know which properties or maps the loop body will mutate before reaching the `JumpLoop` backedge. Maglev handles this with two mechanisms:
1. **`LoopEffects` Tracking (`BeginLoopEffects` / `EndLoopEffects`)**: While building a loop body, `MaglevGraphBuilder::MarkPossibleSideEffect` and `MaglevReducer::RecordKnownProperty` record every mutated property key (`keys_cleared`), context slot (`context_slot_written`), elements-kind transition (`elements_kind_transitioned`), or generic side effect (`unstable_aspects_cleared`) into `LoopEffects* loop_effects_`. When cloning `KnownNodeAspects` for a loop header (`CloneForLoopHeader`), only aspects not invalidated by `loop_effects` are preserved across the loop.
2. **Optimistic Loop Peeling (`--maglev-optimistic-peeled-loops`)**: When peeling a loop, Maglev can speculatively compile the loop body assuming `KnownNodeAspects` holds, and at the backedge check `IsCompatibleWithLoopHeader(backedge_aspects)` (`MergeForLoop`).

---

## 3. Deoptimization Frame Mechanics & Checkpointing

Every Maglev node with `can_eager_deopt()` or `can_lazy_deopt()` carries an **`EagerDeoptInfo`** or **`LazyDeoptInfo`** pointing to a linked list of **`DeoptFrame`** instances (`parent()` chain from innermost inlined frame to outermost top-level function).

### 1. The Four `DeoptFrame` Types (`src/maglev/maglev-ir.h`)
`DeoptFrame` uses a `base::DiscriminatedUnion` (`DeoptFrame::FrameData`) over four frame kinds (`DeoptFrame::FrameType`), all `static_assert`-ed to have `sizeof == sizeof(DeoptFrame)` so they can be passed by value without slicing:

| `DeoptFrame` Subclass | `FrameType` Tag | When Created & Payload (`FrameData`) |
| :--- | :--- | :--- |
| **`InterpretedDeoptFrame`** | `kInterpretedFrame` | Standard JS frame (top-level or inlined). Holds `unit` (`MaglevCompilationUnit`), `CompactInterpreterFrameState* frame_state` (values of all live Ignition registers and accumulator), `closure` (`ValueNode*`), `last_virtual_object` (`VirtualObject*` head of the `VirtualObjectList` at that point), `bytecode_position`, and `source_position`. |
| **`InlinedArgumentsDeoptFrame`** | `kInlinedArgumentsFrame` | Inserted between caller and callee `InterpretedDeoptFrame` when an inlined call passes more or fewer arguments than `unit.parameter_count()` (under/over-application). Holds `unit`, `bytecode_position`, `closure`, and `base::Vector<ValueNode*> arguments`. |
| **`ConstructInvokeStubDeoptFrame`** | `kConstructInvokeStubFrame` | Inserted when inlining a constructor call (`Construct`). Holds `unit`, `source_position`, newly allocated `receiver` (`ValueNode*`), and `context` (`ValueNode*`) so the deoptimizer can reconstruct the `FastConstructStub` frame. |
| **`BuiltinContinuationDeoptFrame`** | `kBuiltinContinuationFrame` | Inserted when lowering an operation into inline code or a sub-call that, upon deoptimization, must finish executing a C++/TFJ builtin continuation (`Builtin builtin_id`, `base::Vector<ValueNode*> parameters`, `context`, and `maybe_js_target`). |

### 2. `latest_checkpointed_frame_`, `VirtualObjectList`, and `CheckpointedJump`
Constructing a fresh `CompactInterpreterFrameState` before every single `CheckMaps` or `CheckedSmiUntag` node would allocate excessive Zone memory when no user-observable state has changed between checks.

* **Lazy Checkpointing (`GetLatestCheckpointedFrame()`)**:
  `MaglevGraphBuilder` caches the most recently constructed `DeoptFrame*` in **`latest_checkpointed_frame_`**. Multiple consecutive eager-deopting checks reuse the exact same `latest_checkpointed_frame_` pointer until a bytecode finishes or a side effect occurs (`ResetBuilderCachedState()` sets `latest_checkpointed_frame_ = nullptr`).
* **`VirtualObjectList` Snapshotting**:
  Because `VirtualObject`s (from inlined allocations or fast literals) can have their fields mutated in place during abstract interpretation, each `VirtualObject` forms an immutable functional linked list versioned in `VirtualObjectList`. Each `InterpretedDeoptFrame` captures `last_virtual_object`, pinning the exact snapshot of all virtual object field values at that checkpoint.
* **`CheckpointedJump` (`OpProperties::DeoptCheckpoint()`)**:
  Passes that run *after* graph building (such as `MaglevPhiRepresentationSelector` when hoisting untagging conversions into loop preheaders) may need to insert new eager-deopting nodes at the end of a basic block where no eager-deopting node previously existed. When `need_checkpointed_loop_entry()` is true (`--maglev-speculative-hoist-phi-untagging`), `MaglevGraphBuilder` emits a **`CheckpointedJump`** (instead of a plain `Jump`) on loop entry edges. `CheckpointedJump` has `has_eager_deopt_info() == true` (`AttachedDeoptInfo::kCheckpoint`) but `can_eager_deopt() == false`: it emits an ordinary unconditional jump at codegen time, while preserving the `EagerDeoptInfo` checkpoint so later passes can attach hoisted checks to it.

### 3. `LazyDeoptResultLocationScope`
Unlike an eager deopt (which restarts execution *at* the current bytecode instruction), a **lazy deopt** occurs *after* a call returns, when the call itself has already completed and produced its return value in the return register (`kReturnRegister0`). The deoptimizer must know which Ignition virtual register (or the accumulator) in the `InterpretedDeoptFrame` should receive that return value instead of reading an old pre-call value from the frame state.

* By default, `MaglevGraphBuilder::GetResultLocationAndSize()` assumes a bytecode's lazy deopt result goes into `interpreter::Register::virtual_accumulator()` (size 1).
* When a single bytecode emits an internal call whose return value is written into a specific register or when a helper overrides where the call result lands before the bytecode finishes, `MaglevGraphBuilder` enters a **`LazyDeoptResultLocationScope(builder, result_location, result_size)`** (`src/maglev/maglev-graph-builder.h`).
* Any `LazyDeoptInfo` created inside that scope records `result_location_` and `result_size_`. During `CompactInterpreterFrameState::ForEachValue` and `AllocateLazyDeopt`, the deopt frame visitor skips marking/spilling the pre-call value in `[result_location, result_location + result_size)` because that slot will be populated by the call's return value upon lazy deoptimization.
