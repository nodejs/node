# Sparkplug (V8 Baseline Compiler)

Sparkplug is V8's non-optimizing baseline JIT compiler (`src/baseline/`). Positioned between the Ignition bytecode interpreter and the Maglev mid-tier optimizing compiler, Sparkplug compiles Ignition `BytecodeArray`s directly into native machine code without constructing an intermediate representation (IR) or performing register allocation.

Its primary engineering goals are:
1. **Ultra-fast compilation speed** (minimal CPU and memory overhead, no Zone-allocated IR graphs).
2. **Elimination of interpreter dispatch overhead** (replacing Ignition's bytecode dispatch loop and operand decoding with direct native execution and embedded immediate constants).
3. **Seamless frame compatibility with Ignition** (allowing instant On-Stack Replacement (OSR) and zero-translation stack walking/deoptimization).

---

## 1. Conceptual Overview: How Sparkplug Works

In Ignition, executing a bytecode instruction requires loading the bytecode opcode from the `BytecodeArray`, looking up its handler in a dispatch table, decoding variable-length register operands at runtime, and jumping to the next handler.

Sparkplug eliminates this overhead by iterating over the function's `BytecodeArray` at compile time and emitting native machine instructions (and direct calls to shared V8 builtins) for each bytecode.

* **No IR**: Bytecodes are translated directly into machine code via `BaselineAssembler` (`src/baseline/baseline-assembler.h`), which wraps the platform `MacroAssembler`.
* **Stack Frame Mirroring**: Sparkplug does not run a register allocator. All Ignition virtual registers (`r0`, `r1`, ...) reside in the exact same stack frame slots as in an Ignition interpreter frame, while the Ignition accumulator lives in a dedicated hardware register (`kInterpreterAccumulatorRegister`, i.e., `rax` on x64, `x0` on arm64).
* **Shared Builtins for Complex Operations**: Simple operations (register moves, constant loads, jumps, returns, and inlined Smi operations in Sparkplug+) are emitted as inline machine instructions. Complex operations (property loads/stores via ICs, generic arithmetic, object allocation, runtime calls) invoke shared baseline builtins using register-based calling conventions.

---

## 2. Two-Pass Compilation Architecture (`BaselineCompiler`)

Although Sparkplug is often described informally as a "single-pass" compiler because it builds no IR, **`BaselineCompiler::GenerateCode()` (`src/baseline/baseline-compiler.cc`) actually executes two linear passes** over the `BytecodeArray`:

```cpp
// src/baseline/baseline-compiler.cc
void BaselineCompiler::GenerateCode() {
  // Pass 1: Pre-visit bytecodes to mark exception handlers and backward jump targets.
  HandlerTable table(*bytecode_);
  for (uint32_t i = 0; i < table.NumberOfRangeEntries(); ++i) {
    MarkIndirectJumpTarget(table.GetRangeHandler(i));
  }
  for (; !iterator_.done(); iterator_.Advance()) {
    PreVisitSingleBytecode();
  }
  iterator_.Reset();

  // Pass 2: Emit prologue and visit each bytecode to emit machine code.
  CodeEntry();
  Prologue();
  AddPosition();
  for (; !iterator_.done(); iterator_.Advance()) {
    VisitSingleBytecode();
    AddPosition();
  }
}
```

### Pass 1: `PreVisitSingleBytecode()` and CFI Landing Pads
Before emitting any machine code, `BaselineCompiler` must know which bytecode offsets are targets of backward jumps (`JumpLoop`) or indirect jumps (exception handlers and OSR/resume entry points):
1. **Exception Handlers**: Every handler offset in the `HandlerTable` is marked via `MarkIndirectJumpTarget(offset)`.
2. **Backward Loop Headers (`JumpLoop`)**: `PreVisitSingleBytecode()` inspects only `Bytecode::kJumpLoop` instructions and calls `EnsureLabel(iterator().GetJumpTargetOffset(), MarkAsIndirectJumpTarget{true})`.
   * Why is `JumpLoop` marked as an *indirect* jump target? On architectures with Control-Flow Integrity (CFI) Landing Pads, such as Intel CET (`endbr64`) or ARM64 Branch Target Identification (`bti`), on-stack replacement (OSR) or generator resumption can jump indirectly into loop headers or exception handlers.
3. **Bitvector & Label Storage**: Instead of allocating a `Label` object for every bytecode offset, `BaselineCompiler` uses a compact `BitVector* label_tags_` of length `2 * bytecode_->length()` alongside a sparse `Label** labels_` array:
   * Bit `2 * offset` records whether `offset` is a jump target (`IsJumpTarget(offset)`), meaning `labels_[offset]` holds an allocated `Label*`.
   * Bit `2 * offset + 1` records whether `offset` requires a CFI landing pad (`IsIndirectJumpTarget(offset)`).
   * **Forward jumps** (`Jump`, `JumpIfTrue`, `JumpIfUndefined`, etc.) do *not* need pre-visiting in Pass 1; they allocate their target `Label` lazily on the fly during Pass 2 via `BuildForwardJumpLabel()` -> `EnsureLabel(target_offset, MarkAsIndirectJumpTarget{false})`.

### Pass 2: `VisitSingleBytecode()`
In Pass 2, `BaselineCompiler` resets the `BytecodeArrayIterator`, emits the function `Prologue()` (which sets up the frame, fills uninitialized register slots with `undefined`, and runs the stack guard check), and walks each bytecode in order:
1. If `IsJumpTarget(offset)` is set, it binds `labels_[offset]`.
2. If `IsIndirectJumpTarget(offset)` is set, it emits a hardware CFI landing pad via `__ JumpTarget()`.
3. It dispatches on `iterator().current_bytecode()` to the corresponding `Visit<BytecodeName>()` method, passing compile-time decoded operands directly into the `MacroAssembler`.
4. After each bytecode (and once after the prologue), it calls `AddPosition()` to record the current machine code offset in `BytecodeOffsetTableBuilder`.

### Concurrent Sparkplug (`BaselineBatchCompiler`)
When `--concurrent-sparkplug` and `--baseline-batch-compilation` are enabled, **`BaselineBatchCompiler`** (`src/baseline/baseline-batch-compiler.h`, `.cc`) avoids compiling functions one by one on the main thread. Instead, it queues `SharedFunctionInfo` handles into a `WeakFixedArray` (`compilation_queue_`) until the cumulative bytecode size exceeds a batch threshold, then dispatches a **`ConcurrentBaselineCompiler`** job to a background thread pool. Each worker uses a `LocalIsolate` to run `BaselineCompiler::GenerateCode()`, and `InstallBatch()` installs the finished `Code` objects on the main thread.

---

## 3. Stack Frame Layout & PC-to-Bytecode Mapping

### `BaselineFrameConstants` vs. `InterpreterFrameConstants`
Sparkplug frames and Ignition interpreter frames share the exact same layout defined in **`UnoptimizedFrameConstants`** (`src/execution/frame-constants.h`):

```
  +--------------------------------------------------+
  |  Incoming Arguments (a_n ... a_0, receiver)      |  fp + kCallerSPOffset + ...
  +--------------------------------------------------+
  |  Return Address (caller PC)                      |  fp + kCallerPCOffset (+8)
  +--------------------------------------------------+
  |  Saved Frame Pointer (caller FP)                 |  fp + 0
  +--------------------------------------------------+
  |  Context (kContextOffset)                        |  fp - 8   (StandardFrameConstants)
  |  JSFunction (kFunctionOffset)                    |  fp - 16
  |  Actual Argument Count (kArgCOffset)             |  fp - 24
  +--------------------------------------------------+
  |  BytecodeArray (kBytecodeArrayFromFp)            |  fp - 32  (UnoptimizedFrameConstants)
  |  BytecodeOffset OR FeedbackCell                  |  fp - 40  <-- THE ONLY DIFFERENCE
  |  FeedbackVector (kFeedbackVectorFromFp)          |  fp - 48
  +--------------------------------------------------+
  |  Register 0 (r0)                                 |  fp - 56  (kRegisterFileFromFp)
  |  Register 1 (r1)                                 |  fp - 64
  |  ...                                             |  ...
  +--------------------------------------------------+
```

The **only structural difference** between `InterpreterFrameConstants` and `BaselineFrameConstants` is extra slot 1 (`kBytecodeOffsetOrFeedbackCellFromFp`, at `fp - 40` on 64-bit platforms):
* In **Ignition** (`InterpreterFrameConstants::kBytecodeOffsetFromFp`), this slot stores the current bytecode offset as a Smi, updated as the interpreter steps through bytecode.
* In **Sparkplug** (`BaselineFrameConstants::kFeedbackCellFromFp`), native execution tracks position via the hardware instruction pointer (`PC`), so storing a bytecode offset on the stack after every instruction would waste CPU cycles. Instead, Sparkplug repurposes this exact stack slot to cache the `JSFunction`'s **`FeedbackCell`** (used for tier-up budget interrupts and lazy feedback vector allocation).

### `BytecodeOffsetTableBuilder` and `BytecodeOffsetIterator`
Because Sparkplug does not store the current bytecode offset in the stack frame, V8 needs a fast, compact way to map a Sparkplug return address (`PC`) back to the corresponding Ignition bytecode offset (and vice versa) during stack walking, exception handling, deoptimization, and OSR between Ignition and Sparkplug.

* **`BytecodeOffsetTableBuilder`** (`src/baseline/baseline-compiler.h`): During Pass 2, `AddPosition()` records the delta between the current assembler `pc_offset()` and `previous_pc_` (`size_t pc_diff = pc_offset - previous_pc_`) and writes it as a base-128 Variable-Length Quantity (`VLQ::WriteUnsigned`) into an off-heap buffer. At the end of compilation, this buffer is copied into a `TrustedByteArray` and attached to the `Code` object (`code_builder.set_bytecode_offset_table(...)`).
* **`BytecodeOffsetIterator`** (`src/baseline/bytecode-offset-iterator.h`): Paired with a `BytecodeArrayIterator`, `BytecodeOffsetIterator` steps through the VLQ-encoded PC deltas in lockstep with the `BytecodeArray`'s instruction lengths. Given a native PC offset (`AdvanceToPCOffset`), it reconstructs the exact bytecode offset; given a target bytecode offset (`AdvanceToBytecodeOffset`), it finds the corresponding native PC offset in the Sparkplug `Code` object.

---

## 4. Builtin Calls, Argument Moving, and Accumulator Preservation

### `CallBuiltin<kBuiltin>` and `MoveArgumentsForBuiltin`
In Ignition, bytecode handlers read their operands from the `BytecodeArray` and frame at runtime. In Sparkplug, the compiler already knows the operand types (`interpreter::Register`, `interpreter::RegisterList`, `uint32_t` index, `Tagged<Smi>`, etc.) at compile time, so it calls baseline-specific builtin stubs using register calling conventions via `BaselineCompiler::CallBuiltin<kBuiltin>(args...)` (`src/baseline/baseline-compiler.cc`).

`detail::MoveArgumentsForBuiltin<kBuiltin>(&basm_, args...)` uses template metaprogramming over `CallInterfaceDescriptorFor<kBuiltin>::type`:
1. **Debug Verification (`CheckArgs` / `CheckSettingDoesntClobber`)**: In `#ifdef DEBUG`, verifies at compile time/generation time that each argument type matches the `CallInterfaceDescriptor`'s `MachineType` and that loading a later register argument from an earlier source does not clobber a hardware register already populated for an earlier argument.
2. **Register Arguments (`ArgumentSettingHelper<Descriptor, ArgIndex, kIsRegister=true, ...>`)**: Iterates through register arguments in the descriptor (`Descriptor::GetRegisterParameter(i)`), emitting moves from frame registers, immediates, or the accumulator into the target hardware registers. If an argument is an `interpreter::RegisterList`, it automatically unpacks the list across register and stack parameters.
3. **Stack Arguments (`ArgumentSettingHelper<..., kIsRegister=false, ...>`)**: Pushes any remaining arguments onto the stack using `basm->Push(...)` or `basm->PushReverse(...)` according to `Descriptor::kStackArgumentOrder` (`StackArgumentOrder::kDefault` vs. `kJS`).
4. **Context Loading**: If `Descriptor::HasContextParameter()` is true, loads the current context from the frame slot (`BaselineFrameConstants::kContextOffset`) into `Descriptor::ContextRegister()` (`kContextRegister`).

### `SaveAccumulatorScope` vs. `EnsureAccumulatorPreservedScope`
The Ignition accumulator resides in the hardware return-value register (`kInterpreterAccumulatorRegister`). Two distinct RAII scopes in `src/baseline/` manage its lifetime:

1. **`SaveAccumulatorScope`** (`src/baseline/baseline-compiler.h`):
   Used in `BaselineCompiler` visitor methods when a bytecode does *not* logically overwrite the accumulator, yet its implementation must call a builtin or runtime function that clobbers `kInterpreterAccumulatorRegister` (for example, `StoreGlobal` or `StaNamedProperty` when saving the accumulator value across a call). It emits `basm_->Push(kInterpreterAccumulatorRegister)` on construction and `basm_->Pop(kInterpreterAccumulatorRegister)` on destruction.
   * **Deopt Invariant (`effect_state_`)**: In `#ifdef DEBUG`, `SaveAccumulatorScope` sets `effect_state_.accumulator_on_stack = true`. Any subsequent call within that scope that might trigger a lazy deopt (`effect_state_.MayDeopt()`) asserts `DCHECK(!accumulator_on_stack)`: because extra values pushed onto the native stack outside the standard frame layout are invisible to the deoptimizer's stack frame reconstruction, a builtin call that can lazily deopt **must never** occur while `SaveAccumulatorScope` has pushed the accumulator onto the stack.
2. **`EnsureAccumulatorPreservedScope`** (`src/baseline/baseline-assembler-inl.h`):
   A debug-only verification scope activated in `BaselineCompiler::VisitSingleBytecode()` when `v8_flags.slow_debug_code` is enabled and the current bytecode neither writes to the accumulator (`!Bytecodes::WritesOrClobbersAccumulator(bytecode)`) nor branches (`!Bytecodes::IsJump(bytecode)` and `!Bytecodes::IsSwitch(bytecode)`). It pushes `kInterpreterAccumulatorRegister` onto the stack before emitting the bytecode's machine instructions, and pops it afterwards into a scratch register to assert (`AssertEqualToAccumulator`) that the emitted code did not accidentally corrupt the accumulator.

---

## 5. Sparkplug+ (`V8_ENABLE_SPARKPLUG_PLUS`)

When V8 is built with `V8_ENABLE_SPARKPLUG_PLUS` and run with `--sparkplug-plus` (provided `local_isolate->is_short_builtin_calls_enabled()` holds, cached in `BaselineCompiler::allow_sparkplug_plus_`), Sparkplug upgrades from a purely unspecialized baseline compiler into a feedback-aware baseline compiler.

### 1. Embedded Feedback & Specialized Builtin Dispatch
Ignition records lightweight type feedback for binary operations, comparisons, and property accesses directly in the `BytecodeArray` or `FeedbackVector`. When `allow_sparkplug_plus_` is true, `BaselineCompiler` inspects this existing feedback at compile time:
* **Binary & Unary Operations (`SelectBuiltinsFromFeedback`)**: Reads the embedded `BinaryOperationFeedback::Type` operand (`iterator().GetEmbeddedFeedback(feedback_index)`) and selects a type-specialized baseline builtin stub (e.g., `Builtin::kAdd_SignedSmall_Baseline` or `Builtin::kAdd_Number_Baseline` instead of generic `Builtin::kAdd_Baseline`). If the fast stub encounters an unexpected type at runtime, it updates the embedded feedback in the `BytecodeArray` and tails into the generic path.
* **Comparisons (`SelectCompareBuiltinsFromFeedback`)**: Dispatches to `Equal_SignedSmall_Baseline`, `StrictEqual_Receivers_Baseline`, `LessThan_Number_Baseline`, etc., based on `CompareOperationFeedback::Type`.
* **Property Accesses (`TryEmitFastLoadNamedProperty` / `TryEmitFastLoadKeyedProperty`)**: Checks the `FeedbackVector` slot state at compile time and calls specialized baseline IC stubs (`LoadICUninitializedBaseline` vs. `LoadICGenericBaseline`) rather than always going through the full IC dispatcher.

### 2. Inline Smi Fast Paths (`--sparkplug-inline-smi`)
When `v8_flags.sparkplug_inline_smi` is enabled alongside `allow_sparkplug_plus_` and the embedded feedback at compile time is `BinaryOperationFeedback::kSignedSmall` (or `CompareOperationFeedback::kSignedSmall`), Sparkplug+ emits **inline Smi machine code** directly into the baseline instruction stream (`src/baseline/baseline-compiler.cc`):
* **`TryEmitInlineSmiBinaryOperation`**: For `AddSmi`, `SubSmi`, `BitwiseAndSmi`, `BitwiseOrSmi`, `BitwiseXorSmi`, `ShiftLeftSmi`, `ShiftRightSmi`, and `ShiftRightLogicalSmi`, emits an inline Smi tag check (`JumpIfNotSmi(kInterpreterAccumulatorRegister, &slow_path)`) followed by the direct hardware instruction (`SmiAddConstant`, `And`, `SmiShiftLeftConstant`, etc.) with overflow check jumping to the out-of-line builtin call `slow_path`.
* **`TryEmitInlineSmiUnary`**: Emits inline Smi check + `SmiAddConstant(..., 1)` / `SmiSubConstant(..., 1)` for `Inc` and `Dec`.
* **`TryEmitInlineSmiCompare` and Branch Fusion (`GetCompareBranchCandidate`)**: Before emitting `TestEqual`, `TestEqualStrict`, `TestLessThan`, `TestGreaterThan`, `TestLessThanOrEqual`, or `TestGreaterThanOrEqual` with `kSignedSmall` feedback, `GetCompareBranchCandidate()` peeks ahead in the `BytecodeArrayIterator` (`iterator().PeekNextBytecode()`). If the immediately following bytecode is `JumpIfTrue` or `JumpIfFalse` and its target offset is not itself a separate jump target (`!IsJumpTarget(next_offset)`), Sparkplug+ **fuses the comparison and conditional branch** into a single inline Smi check and conditional jump, advancing the bytecode iterator past both instructions without materializing a boolean `Oddball` (`true`/`false`) into the accumulator.
