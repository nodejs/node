# Liftoff (WebAssembly Baseline Compiler)

Liftoff (`src/wasm/baseline/`) is V8's single-pass baseline JIT compiler for WebAssembly (`ExecutionTier::kLiftoff` in `src/wasm/wasm-tier.h`). Positioned ahead of the Turboshaft optimizing tier (`ExecutionTier::kTurbofan`; see [architecture.md](architecture.md)), Liftoff prioritizes fast startup and low compilation memory overhead while emitting register-allocated machine code in a single linear pass over WebAssembly wire bytes.

Beyond baseline execution and call-site type feedback collection (`--wasm-inlining`), Liftoff is the sole WebAssembly tier supporting interactive debugging (`DebugSideTable`) and serves as the unoptimized target when speculative Turboshaft code deoptimizes (`--wasm-deopt`).

---

## 1. Overview & Single-Pass Decoder Interface

### 1. `WasmFullDecoder` Single-Pass Pipeline
`ExecuteLiftoffCompilation` (`src/wasm/baseline/liftoff-compiler.cc`) drives code generation by instantiating `WasmFullDecoder<Decoder::NoValidationTag, LiftoffCompiler>` (`src/wasm/function-body-decoder-impl.h`):

```
  Validated Wasm Function Wire Bytes
                 |
                 v
  WasmFullDecoder<Decoder::NoValidationTag, LiftoffCompiler>
    - StartFunction / StartFunctionBody (frame setup, ParameterProcessor, local init)
    - Per-opcode callbacks -> ad-hoc regalloc (CacheState) + LiftoffAssembler (no IR)
    - FinishFunction (epilogue, OutOfLineCode, PatchPrepareStackFrame, SafepointTable)
                 |
                 v
  WasmCompilationResult (instr_buffer, CodeDesc, source positions, trap metadata)
```

- **No IR and No Pre-Pass**: Function wire bytes are validated during module creation or streaming decoding (or just prior to compilation in `WasmCompilationUnit::ExecuteCompilation` when `Validation` is `kMustValidate`). Running with `Decoder::NoValidationTag`, `WasmFullDecoder` decodes opcodes sequentially and invokes `LiftoffCompiler` interface callbacks that emit native machine instructions directly through `LiftoffAssembler`.
- **Ad-Hoc Single-Pass Register Allocation**: Register allocation is merged directly into the single decoding pass via `LiftoffAssembler::CacheState` rather than running a separate liveness or linear-scan pass. As opcodes stream through the decoder, Liftoff tracks locals and operand-stack values in cache registers (`kRegister`, reference-counted for zero-copy `local.get` / `local.tee` aliasing) or as unmaterialized small constants (`kIntConst`), spills lazily on register pressure or before calls and loop headers, and reconciles divergent states at control-flow joins via `ParallelMove` (see Section 2).
- **Structured Control Flow**: WebAssembly's structured control constructs (`block`, `loop`, `if`/`else`/`end`, `try`/`catch`, `try_table`, `br`, `br_if`, `br_table`) eliminate any bytecode pre-walk for jump targets. Every forward branch targets an enclosing `Control` entry on the decoder's control stack (`c->label`, `c->label_state`), and every backward branch targets an already-bound `loop` header (`loop->label`, `loop->label_state`).

### 2. Bailouts (`LiftoffBailoutReason`) and Tier Fallback
If compilation hits an unsupported architecture (`kUnsupportedArchitecture`), missing CPU feature (`kMissingCPUFeature`), or unimplemented instruction (`kComplexOperation` or proposal-specific reasons in `src/wasm/baseline/liftoff-bailout-reasons.h`), `LiftoffAssembler::bailout` / `LiftoffCompiler::unsupported` records the `LiftoffBailoutReason` and aborts decoding:

- **Fallback to Turboshaft**: `WasmCompilationUnit::ExecuteCompilation` returns the failed `WasmCompilationResult`, and callers -- `ExecuteCompilationUnits` and `CompileLazy` (`src/wasm/module-compiler.cc`) as well as `WasmCompilationUnit::CompileWasmFunction` (`src/wasm/function-compiler.cc`) -- transparently retry compilation with Turboshaft (`ExecutionTier::kTurbofan`).
- **Fatal Bailouts**: `CheckBailoutAllowed` (`src/wasm/baseline/liftoff-compiler.cc`) makes any non-`kDecodeError` bailout `FATAL` under `--liftoff-only` (and on all but a few community ports whenever no experimental Wasm features are enabled, except `kMissingCPUFeature` and `--enable-testing-opcode-in-wasm`). Similarly, `RecompileLiftoffWithBreakpoints` (`src/wasm/wasm-debug.cc`) treats any Liftoff compilation failure as `FATAL` because Turboshaft cannot emit breakpoints.

---

## 2. Abstract Value State, Register Allocation & Control-Flow Merges

### 1. `LiftoffAssembler::CacheState` and `LiftoffVarState`
Instead of global liveness analysis, `LiftoffAssembler` tracks abstract execution state in `LiftoffAssembler::CacheState` (`src/wasm/baseline/liftoff-assembler.h`):

- **Unified Value Stack (`CacheState::stack_state`)**: A contiguous vector of `LiftoffVarState` entries unifying WebAssembly parameters and locals at indices `[0, num_locals_)` with active operand-stack values (and implicit caught exception references in `catch` handlers) from `num_locals_` upward.
- **`LiftoffVarState` (`src/wasm/baseline/liftoff-varstate.h`)**: Each entry records one slot's `ValueKind`, `Location`, payload, and stack-frame spill offset:

| `LiftoffVarState::Location` | Active Payload | Meaning |
|---|---|---|
| `kStack` | `offset()` | Value resides in the stack frame at `fp - offset()`. |
| `kRegister` | `reg()` (`LiftoffRegister`) | Value resides in a cache register (or register pair); `offset()` reserves its home slot if later spilled. |
| `kIntConst` | `i32_const()` (`int32_t`) | Unmaterialized constant representable via `int32_t`: `kI32`, sign-extended 32-bit `kI64`, raw-bit `kF32`, or `kF64` bit patterns representable as sign-extended `int32_t` (notably `+0.0`). |

- **Position-Determined Spill Offsets**: Assigned deterministically from the kinds below on the stack via `NextSpillOffset` (growing downward from `fp` past `StaticStackFrameSize()`, aligned when `NeedsAlignment(kind)` holds, and re-packed across merge regions in `InitMergeRegion`). Spilling a slot writes to `fp - offset()` and transitions its location to `kStack` without shifting neighboring slots.

### 2. Cache Register Pools, Reference Counting & Spill Victim Selection
- **Register Pools & Cross-Component Coupling**: Allocatable registers are defined per architecture by `kLiftoffAssemblerGpCacheRegs` and `kLiftoffAssemblerFpCacheRegs` (`src/wasm/baseline/liftoff-assembler-defs.h`), wrapped as `LiftoffRegList`s (`kGpCacheRegList`, `kFpCacheRegList` in `src/wasm/baseline/liftoff-register.h`), with `LiftoffRegister::ForPair` (`kNeedI64RegPair`) and `ForFpPair` (`kNeedS128RegPair`) on 32-bit or paired-FP targets. Two external invariants constrain these lists:
  1. `WasmDebugBreakFrameConstants::kPushedGpRegs` and `kPushedFpRegs` must match the Liftoff cache register lists (`static_assert`ed in `test/unittests/wasm/liftoff-register-unittests.cc`) so `WasmDebugBreak` frames preserve and trace all live cache registers.
  2. Under `--wasm-deopt`, every Liftoff cache register must also be allocatable in `RegisterConfiguration::Default()` so the deoptimizer can materialize Liftoff register state.
- **Zero-Copy Aliasing (`used_registers` & `register_use_count`)**: Operations such as `local.get` and `local.tee` push a `kRegister` `LiftoffVarState` sharing the existing register and increment `CacheState::register_use_count` (alongside the `used_registers` bitmask) without emitting moves. Modifying an operand in place uses `LoadToModifiableRegister` / `PopToModifiableRegister` to copy into an unused register whenever the register still has other uses (`!is_free(reg)`) or is `pinned`.
- **Cached Special Registers**: `CacheState` opportunistically caches `WasmTrustedInstanceData` in `cached_instance_data` (preferring `kWasmImplicitArgRegister` via `TrySetCachedInstanceRegister`) and a linear-memory base pointer in `cached_mem_start` (paired with `cached_mem_index`).
- **Spill Victim Selection (`GetUnusedRegister` -> `SpillOneRegister`)**:
  1. If `candidates` contains a volatile cache register (`has_volatile_register`: `cached_instance_data` or `cached_mem_start`), `take_volatile_register` reclaims it immediately without a store, as both can be reloaded from the frame or instance.
  2. Otherwise, `CacheState::GetNextSpillReg` selects a victim round-robin over `candidates`, and `SpillRegister` writes all `stack_state` slots overlapping that register to their spill offsets.

### 3. Control-Flow Merges, Loop Headers & `ParallelMove`

```
  Source stack_state:  |--- locals ---|--- stack prefix ---|--- (discarded) ---|--- merge (arity) ---|
  Target label_state:  |--- locals ---|--- stack prefix ---|--- merge (arity) ---|
```

- **First Forward Merge (`MergeIntoNewState`)**: The first path reaching an unbound merge point calls `InitMergeRegion` (`src/wasm/baseline/liftoff-assembler.cc`) to establish `Control::label_state` (`BrIf` calls `PrepareForBranch` before its conditional jump so taken and fallthrough paths share materialized transfers):
  - `locals` and single-value `merge` (`arity == 1`) materialize `kIntConst` slots and deduplicate aliased registers so incoming branches have distinct register targets.
  - The unchanged `stack prefix` preserves `kIntConst` entries and shared registers (`kConstantsAllowed`, `kReuseRegisters`).
  - Multi-value `merge` regions (`arity > 1`) force all merge slots to `kStack` (`kRegistersNotAllowed`, re-packed right after the prefix) because simultaneous stack-to-stack moves and register reloads could otherwise conflict in `ParallelMove`.
- **Subsequent Merges (`MergeFullStackWith` / `MergeStackWith`)**: On `kForwardJump`, later paths transfer their `stack_state` into `target` and clear `target.cached_instance_data` or `target.cached_mem_start` on mismatch. On `kBackwardJump`, the loop-header state is already bound and fixed, so `MergeStackWith` moves or reloads `cached_instance_data` / `cached_mem_start` (`RestoreCachedRegisters`) to match the header.
- **Loop Headers (`Loop`)**: Before binding `loop->label` and snapshotting `loop->label_state`, `LiftoffCompiler::Loop` calls `SpillLocals()` and `SpillLoopArgs(loop->start_merge.arity)`. Canonicalizing locals and loop inputs to `kStack` frees cache registers inside the loop body and guarantees backward branches never conflict on register assignments.
- **`ParallelMove` (`src/wasm/baseline/parallel-move.{h,cc}`)**: `Transfer` emits stack stores eagerly, records register-to-register moves, and queues constant and stack-slot loads; `Execute` orders register moves so unblocked destinations run first, breaks register move cycles through a temporary stack slot, and emits constant/stack loads last.

---

## 3. Frame Layout, Calls & Platform Backend Split

### 1. Frame Layout & Deferred Frame Setup
Liftoff frames use `StackFrame::WASM` (`WasmLiftoffFrameConstants` in `src/execution/<arch>/frame-constants-<arch>.h` and `wasm::kOSRTargetOffset` in `src/wasm/wasm-constants.h`):

```
  high addr  [fp + kSystemPointerSize]  Caller return PC
             [fp + 0]                   Saved caller fp
             [fp - 1 * ptr]             Frame marker: StackFrame::WASM (kFrameTypeOffset)
             [fp - 2 * ptr]             WasmTrustedInstanceData (kInstanceDataOffset)
             [fp - 3 * ptr]             Feedback vector FixedArray (kFeedbackVectorOffset; populated if --wasm-inlining)
             [fp - 4 * ptr]             OSR target PC (wasm::kOSRTargetOffset, x64 only)
             [fp - offset]              Locals & operand-stack spill slots (StaticStackFrameSize..max_used_spill_offset_)
  low addr   [sp ..]                    OOL register-push spill area & outgoing stack call arguments
```

- **`WasmLiftoffFrameSetup` Stub**: In `StartFunctionBody`, when `--wasm-inlining` is enabled, `CallFrameSetupStub` loads the defined function index into `kLiftoffFrameSetupFunctionReg` and calls `Builtin::kWasmLiftoffFrameSetup`. The stub constructs the frame, lazily allocates the function's feedback vector under a temporary `StackFrame::WASM_LIFTOFF_SETUP` marker (`WasmLiftoffSetupFrameConstants`) if still unallocated, increments the invocation count, and populates the instance and feedback-vector slots. Without `--wasm-inlining`, Liftoff emits `EnterFrame(StackFrame::WASM)` directly.
- **Entry `ParameterProcessor` & Local Initialization**: `ParameterProcessor` populates parameter `stack_state` entries, clearing upper bits of 64-bit `kI32` register parameters (`clear_i32_upper_half`) and re-applying the pointer-compression cage base (`DecompressTagged`) to reference parameters under `V8_ENABLE_SANDBOX`. Non-parameter `i32`/`i64`/`f32`/`f64` locals start as unmaterialized `kIntConst(0)` unless `SpillLocalsInitially` triggers (`actual_locals > kNumCacheRegisters / 2` or any non-parameter local has another kind such as `kS128` or a reference, initialized on the stack using `RootIndex::kNullValue` / `RootIndex::kWasmNull` for references).
- **`PrepareStackFrame` and `PatchPrepareStackFrame`**: Because `max_used_spill_offset_` and `max_pushed_argument_slots_` are only known after decoding the entire body, `PrepareStackFrame()` reserves a fixed-width `sp`-subtraction placeholder at `pc_offset_stack_frame_construction_`. In `FinishFunction` (after `AlignFrameSize()`), `PatchPrepareStackFrame` patches the subtraction in place for small frames, or patches an unconditional jump to an out-of-line extended stack check (`Builtin::kWasmStackOverflow`, or `Builtin::kWasmHandleStackOverflow` under `--wasm-growable-stacks`) appended at the end of the code buffer for large frames.

### 2. Call Lowering & Platform Backend Split
- **Direct & Builtin Calls (`PrepareCall` / `FinishCall`)**: `LiftoffAssembler::PrepareCall` (`src/wasm/baseline/liftoff-assembler.cc`) drops argument slots, spills all remaining live cache registers via `SpillAllRegisters()` before parameter registers can be clobbered, constructs outgoing stack arguments (`LiftoffStackSlots::Construct`), moves register arguments and `kWasmImplicitArgRegister` (`kGpParamRegisters[0]`) via `ParallelMove`, and reloads the instance from the frame if needed. After the call, `LiftoffCompiler::FinishCall` defines the safepoint, records debug/deopt/OSR/landing-pad state, and delegates to `LiftoffAssembler::FinishCall` to push return registers (`clear_i32_upper_half` on 64-bit `kI32` returns, plus `DecompressTagged` on reference returns when `IsAnyWasmFunctionCall()` under `V8_ENABLE_SANDBOX`). Intra-module calls (`CallNativeWasmCode`) relocate through the `NativeModule` jump table, imported calls load `(implicit_arg, target)` from `WasmDispatchTableForImports`, and internal helpers use `PrepareBuiltinCall`.
- **Indirect & Reference Calls (`call_indirect`, `call_ref`)**: `CallIndirectImpl` bounds-checks the table index against `WasmDispatchTable` (`dispatch_table0()` or `dispatch_tables()`), checks the entry against `-1` (`kThrowWasmTrapNullFunc`) and verifies the canonical signature ID (`WasmDispatchTable::kSigBias`) or RTT supertype chain (`kThrowWasmTrapFuncSigMismatch`), then loads `(implicit_arg, target)` and emits `CallIndirect` -> `CallWasmCodePointer` (which additionally checks `signature_hash()` inside `WasmCodePointerTable` under `V8_ENABLE_SANDBOX`). For call-site feedback, `CallRefImpl` calls `Builtin::kCallRefIC` under `--wasm-inlining`, and `CallIndirectImpl` calls `Builtin::kCallIndirectIC` under `--wasm-inlining-call-indirect`.
- **Tail Calls (`return_call*`)**: `PrepareTailCall` shifts outgoing stack arguments over the current frame's incoming parameter slots, restores caller `fp` and return address, and jumps via `TailCallNativeWasmCode` or `TailCallIndirect`.
- **Platform Backend Split**: Shared compiler and `CacheState` logic lives in `liftoff-compiler.cc`, `liftoff-assembler.{h,cc}`, and `liftoff-assembler-inl.h`. Architecture-specific emission (`Spill`, `Fill`, `Load`, `Store`, `CheckTierUp`, arithmetic, and SIMD instructions) is implemented inline in `src/wasm/baseline/<arch>/liftoff-assembler-<arch>-inl.h` (primary targets: `x64`, `arm64`, `ia32`, and `arm`).

---

## 4. GC Safepoints, Out-of-Line Code & Traps

### 1. Precise Root Tracking & `SafepointTableBuilder`
- **`tagged_locals_slots_`**: Liftoff tracks GC roots precisely for all `is_reference(kind)` slots. After initializing locals in `StartFunctionBody`, `RecordTaggedLocalSlots()` precomputes a `GrowableBitVector tagged_locals_slots_` of safepoint slot indices for reference-typed locals in `[0, num_locals_)`.
- **`SafepointTableBuilder` (`src/codegen/safepoint-table.h`)**: `LiftoffCompiler` owns `SafepointTableBuilder safepoint_table_builder_(zone)` and emits the XOR-delta-compressed safepoint table in `FinishFunction`:
  - **`DefineSafepoint`**: Used after normal calls where `SpillAllRegisters()` has spilled all live values. Marks `is_reference` operand-stack slots (`[num_locals_, stack_height())`) via `DefineTaggedStackSlot` and bulk-merges `tagged_locals_slots_` via `DefineTaggedStackSlots`.
  - **`DefineSafepointWithCalleeSavedRegisters`**: Used at `Builtin::kWasmDebugBreak` and debug trap sites where the callee pushes all Liftoff cache registers (`WasmDebugBreakFrameConstants`); records both `kStack` and `kRegister` reference slots (plus `cached_instance_data`) via `DefineTaggedRegister`.

### 2. Exception Landing Pads, `OutOfLineCode` & Hardware Trap Metadata
- **Exception Landing Pads (`EmitLandingPad` & `HandlerTable`)**: Calls inside `try` or `try_table` scopes emit an inline landing pad (`EmitLandingPad`) recorded in `handlers_` (`HandlerTable::EmitReturnEntry` in `FinishFunction`). On unwind, the landing pad calls `PushException()` (`kReturnRegister0`), merges into `TryInfo::catch_state`, and jumps to `TryInfo::catch_label`.
- **`OutOfLineCode` (`GenerateOutOfLineCode`)**: Cold paths (`OutOfLineCode::Trap`, `OutOfLineCode::StackCheck`, and `OutOfLineCode::TierupCheck`) are queued during decoding (with `FreezeCacheState` / `FREEZE_STATE` guarding in `DEBUG` builds against unintended `CacheState` mutations across internal conditional branches) and emitted after the function epilogue:
  - `StackCheck` and `TierupCheck` preserve live registers with `PushRegisters(regs_to_save)` (tracked in `ool_spill_space_size_` / `OolSpillCount()`), record pushed reference slots in the safepoint via `GetTaggedSlotsForOOLCode(..., SpillLocation::kTopOfStack)` and `RecordSpillsInSafepoint`, pop registers, and jump back to `ool->continuation`.
  - Non-debug traps never return and need no safepoint roots; `AddOutOfLineTrap` queues them separately after safepoint-bearing OOL stubs so their empty safepoint entries deduplicate.
- **`TrappingInstructionData`**: For hardware-protected linear-memory accesses and implicit `WasmNull` dereferences (see [architecture.md](architecture.md)), `RegisterTrappingInstruction` appends `trap_handler::TrappingInstructionData` to `trapping_instructions_` with a source position (plus a `DefineSafepointWithCalleeSavedRegisters` entry at `trapping_instruction_pc + WasmFrameConstants::kTrappingInstructionReturnAddressOffset` when `for_debugging_` is set).

---

## 5. Tiering, Debugging, OSR & Deoptimization Invariants

### 1. Tier-Up Budget, `ForDebugging` Modes & Debug OSR
- **Tier-Up Budget (`CheckTierUp` / `TierupCheck`)**: Under `--wasm-dynamic-tiering` (`for_debugging_ == kNotForDebugging`), Liftoff decrements the function's entry in `WasmTrustedInstanceData::tiering_budget_array()` (`kTieringBudgetArrayOffset`) at loop back-edges (`BrImpl`), returns (`ReturnImpl`), and tail calls (`TierupCheckOnTailCall`). When negative, `OutOfLineCode::TierupCheck` calls `Builtin::kWasmTriggerTierUp` (`TriggerTierUp` in `src/wasm/module-compiler.cc`) to enqueue background Turboshaft compilation; the `NativeModule` jump table is patched later when that Turboshaft code is published (Wasm does not perform Liftoff-to-Turboshaft loop OSR; when `!dynamic_tiering()`, `Loop` emits a header `StackCheck` instead).
- **`ForDebugging` & `DebugSideTable` (`src/wasm/wasm-debug.{h,cc}`)**: `ForDebugging` (`src/wasm/wasm-tier.h`) selects `kNotForDebugging`, `kForDebugging` (tiered-down inspectable code that spills register-held slots to their home frame offsets via `GetSpilledRegistersForInspection` and routes OOL traps through `Builtin::kWasmTrapHandlerThrowTrap`), `kWithBreakpoints` (`RecompileLiftoffWithBreakpoints` emitting `Builtin::kWasmDebugBreak`), or `kForStepping` (flooded with breakpoints via sentinel offset `0`). `DebugSideTable` delta-encodes per-PC value locations (`kConstant`, `kRegister`, `kStack`); for `kForDebugging` and `kForStepping`, V8 generates it lazily on inspection via `GenerateLiftoffDebugSideTable`.
- **Debug OSR (`UpdateReturnAddress` / `MaybeOSR`)**: Setting or clearing breakpoints redirects active `for_debugging()` frames across Liftoff debug variants (`PointerAuthentication::ReplacePC`, or writing `fp - wasm::kOSRTargetOffset` on `x64` for CET shadow-stack compatibility, consumed by `MaybeOSR` -> `Builtin::kWasmOnStackReplace`). Non-debugging frames are never OSR-patched into debugging code because `GetSpilledRegistersForInspection` spills register-held slots to their home frame offsets in OOL stubs, giving debugging code a strictly larger regular frame (and a different tagged-slot map) than non-debugging code.

### 2. Deoptimization (`--wasm-deopt`) & Cross-Compilation Contracts
When speculative Turboshaft code deoptimizes (see [architecture.md](architecture.md)), `Deoptimizer::DoComputeWasmLiftoffFrame` -> `CompileWithLiftoffAndGetDeoptInfo` (`src/deoptimizer/deoptimizer.cc`) invokes `ExecuteLiftoffCompilation` for each inlined frame with `deopt_info_bytecode_offset` and `LocationKindForDeopt` (`kEagerDeopt` in `EmitDeoptPoint` for the innermost call site, or `kInlinedCall` in `StoreFrameDescriptionForDeopt` for outer inlined callers), producing `LiftoffFrameDescriptionForDeopt` (`src/wasm/wasm-deopt-data.h`) and publishing the Liftoff `WasmCode` (`for_deopt = true`).

Four invariants span separate Liftoff compilations of the same function:
1. **`FunctionTypeFeedback::liftoff_frame_size`**: Recorded in `FinishFunction` and read by Turboshaft's `GetLiftoffFrameSize` (`src/wasm/turboshaft-graph-interface.cc`) to size deopt frame states; a deopt-time Liftoff recompile must produce the exact same frame size.
2. **Feedback Slot Numbering (`call_targets`)**: `encountered_call_instructions_` must match identically across recompiles (`DCHECK_EQ` in `FinishFunction`).
3. **Lazy `DebugSideTable` Regeneration**: `GenerateLiftoffDebugSideTable` recompiles the function and must reproduce the exact PC offsets and slot placements of the running `kForDebugging` / `kForStepping` code.
4. **Breakpoint-Independent Debug OSR Layout**: Frame size and safepoint tagged-slot maps across `for_debugging()` recompiles must not depend on the active breakpoint set (`src/wasm/wasm-debug.cc`).

---

## 6. Compilation Memory Lifecycle & Publishing

- **Per-Function `Zone` and `AssemblerBuffer`**: `ExecuteLiftoffCompilation` allocates a temporary per-function `Zone` (on `GetWasmEngine()->allocator()`) holding `CacheState`, decoder control blocks, `OutOfLineCode` lists, `SafepointTableBuilder`, `SourcePositionTableBuilder`, and `trapping_instructions_`; this `Zone` is destroyed when `ExecuteLiftoffCompilation` returns. Machine code is emitted into a heap-allocated `AssemblerBuffer` (`NewLiftoffAssemblerBuffer`, sized via `WasmCodeManager::EstimateLiftoffCodeSize`).
- **`WasmCompilationResult` (`src/wasm/function-compiler.h`)**: Packages `code_desc`, ownership of `instr_buffer`, serialized `source_positions`, `trapping_instructions_data`, frame/GC slot counts, and optional `liftoff_frame_descriptions`.
- **`NativeModule::AddCompiledCode` & `PublishCode` (`src/wasm/wasm-code-manager.cc`)**: Copies `instr_buffer` and metadata into executable `NativeModule` code space, patches `RelocInfo` (`WASM_CALL`, `WASM_STUB_CALL`), flushes the instruction cache, drops `reloc_info` (Liftoff code is neither relocated nor serialized by `WasmSerializer`), and publishes the resulting `WasmCode` into the module's code and jump tables (see [architecture.md](architecture.md)).

---

## 7. Key Files Reference

| File | Purpose |
|---|---|
| `src/wasm/baseline/liftoff-compiler.{h,cc}` | `LiftoffCompiler` decoder callbacks, `OutOfLineCode`, `FinishCall`, `ExecuteLiftoffCompilation`, and `GenerateLiftoffDebugSideTable`. |
| `src/wasm/baseline/liftoff-assembler.{h,cc}`, `-inl.h` | `LiftoffAssembler`, `CacheState`, spill/merge algorithms (`MergeIntoNewState`, `MergeStackWith`), and `PrepareCall`. |
| `src/wasm/baseline/<arch>/liftoff-assembler-<arch>-inl.h` | Platform-specific `LiftoffAssembler` implementations (`x64`, `arm64`, `ia32`, `arm`, plus community ports). |
| `src/wasm/baseline/liftoff-varstate.h` | `LiftoffVarState` (`kStack`, `kRegister`, `kIntConst`, spill offset). |
| `src/wasm/baseline/liftoff-register.h`, `liftoff-assembler-defs.h` | `LiftoffRegister`, `LiftoffRegList`, `kLiftoffAssemblerGpCacheRegs`, and `kLiftoffAssemblerFpCacheRegs`. |
| `src/wasm/baseline/parallel-move.{h,cc}`, `-inl.h` | `ParallelMove` dependency ordering and cycle-breaking spill/reload resolution. |
| `src/wasm/baseline/liftoff-bailout-reasons.h` | `LiftoffBailoutReason` enum and histogram mapping. |
| `src/execution/<arch>/frame-constants-<arch>.h`, `src/wasm/wasm-constants.h` | `WasmLiftoffFrameConstants`, `WasmLiftoffSetupFrameConstants`, and `wasm::kOSRTargetOffset`. |
| `src/wasm/function-compiler.{h,cc}`, `wasm-code-manager.{h,cc}` | `WasmCompilationUnit`, `WasmCompilationResult`, `NativeModule::AddCompiledCode`, and `PublishCode`. |
| `src/wasm/wasm-debug.{h,cc}`, `src/wasm/wasm-deopt-data.h`, `src/deoptimizer/deoptimizer.cc` | `DebugSideTable`, `RecompileLiftoffWithBreakpoints`, `UpdateReturnAddress`, `LiftoffFrameDescriptionForDeopt`, and `CompileWithLiftoffAndGetDeoptInfo`. |
