# Ignition: V8's Bytecode Interpreter

Ignition is V8's baseline execution tier. It compiles the Abstract Syntax Tree (AST) produced by the parser into a compact, register-based bytecode (`BytecodeArray`) and executes it via a threaded-code interpreter whose bytecode handlers are compiled into native machine code at snapshot build time.

Ignition serves three primary architectural roles:
1. **Low memory overhead**: Bytecode is significantly smaller than baseline machine code, especially for cold or run-once functions.
2. **Fast startup and lazy compilation**: AST-to-bytecode compilation is a single linear pass over the AST without complex control-flow graphs or register allocation passes.
3. **Canonical source of truth for higher tiers**: Sparkplug (baseline compiler), Maglev (mid-tier SSA compiler), and Turbofan/Turboshaft (optimizing compiler) all compile from Ignition bytecode rather than re-parsing JavaScript source or walking the AST.

---

## 1. The 4-Layer Bytecode Generation Pipeline

When `Interpreter::NewCompilationJob` (`src/interpreter/interpreter.cc`) compiles an AST `FunctionLiteral`, bytecode generation flows through a strict four-layer pipeline before producing an on-heap `BytecodeArray` in `TrustedSpace`:

```
FunctionLiteral (AST)
   │
   ▼
1. BytecodeGenerator          (src/interpreter/bytecode-generator.{h,cc})
   │  AstVisitor that walks statements/expressions, manages lexical/register
   │  scopes, control-flow labels, and TypeHints.
   ▼
2. BytecodeArrayBuilder       (src/interpreter/bytecode-array-builder.{h,cc})
   │  High-level bytecode emission API; allocates ConstantPool entries and
   │  HandlerTable entries, and routes register transfers to the optimizer.
   ▼
3. BytecodeRegisterOptimizer  (src/interpreter/bytecode-register-optimizer.{h,cc})
   │  Maintains lazy equivalence sets across virtual_accumulator and registers
   │  to elide redundant Ldar / Star / Mov instructions and temporary registers.
   ▼
4. BytecodeArrayWriter        (src/interpreter/bytecode-array-writer.{h,cc})
   │  Encodes BytecodeNodes into raw bytes, scales operands (Single/Wide/ExtraWide),
   │  patches jumps, elides dead code, and builds the SourcePositionTable.
   ▼
5. BytecodeVerifier::Verify   (src/sandbox/bytecode-verifier.{h,cc})
      Validates control-flow targets and (in full mode) register/operand bounds
      before marking the BytecodeArray verified for execution inside the sandbox.
```

### Layer 1: `BytecodeGenerator` (`src/interpreter/bytecode-generator.cc`)
`BytecodeGenerator` subclasses `AstVisitor<BytecodeGenerator>`. Compilation is split into two phases:
- `GenerateBytecode(uintptr_t stack_limit)`: Can execute off-thread (parked via `LocalIsolate::ParkIfOnBackgroundAndExecute`). It allocates slots for parameters and locals, sets up the function prologue (context allocation, stack checks), visits the AST body, and records deferred heap constants in `AstValueFactory`.
- `FinalizeBytecode(IsolateT* isolate, Handle<Script> script)`: Runs on the main/local isolate thread, internalizes strings and allocates heap constants (`AllocateDeferredConstants`), finalizes the `BytecodeArray` via `builder()->ToBytecodeArray(isolate)`, and constructs the `FeedbackVectorSpec`.

### Layer 2: `BytecodeArrayBuilder` (`src/interpreter/bytecode-array-builder.h`)
`BytecodeArrayBuilder` provides a semantic C++ interface for emitting instructions (`LoadLiteral`, `LoadAccumulatorWithRegister`, `StoreAccumulatorInRegister`, `MoveRegister`, `BinaryOperation`, `JumpIfTrue`, `CallProperty`, etc.) without exposing raw opcode variants or operand widths:
- Selects specialized short opcodes automatically (e.g. `LdaZero`, `LdaSmi`, `LdaUndefined`, `LdaTrue`, `CallProperty0..2`).
- Translates `Register` and `RegisterList` arguments through `GetInputRegisterOperand` and `GetOutputRegisterOperand`, consulting `BytecodeRegisterOptimizer` when `v8_flags.ignition_reo` is enabled.
- Constructs `BytecodeNode` objects (via `BytecodeNodeBuilder::Make`) carrying the opcode, up to 5 raw `uint32_t` operands, and an optional `BytecodeSourceInfo`.

### Layer 3: `BytecodeRegisterOptimizer` (`src/interpreter/bytecode-register-optimizer.h`)
Rather than emitting `Ldar`, `Star`, and `Mov` instructions into the `BytecodeArrayWriter` immediately, `BytecodeArrayBuilder` forwards register transfers (`DoLdar`, `DoStar`, `DoMov`) to `BytecodeRegisterOptimizer`. The optimizer tracks which registers and the accumulator currently hold the same value, materializing transfers lazily only when an instruction consumes or overwrites a register or when control flow merges. See [Section 3](#3-bytecoderegisteroptimizer-and-register-equivalence-sets) for the detailed algorithm.

### Layer 4: `BytecodeArrayWriter` and `BytecodeVerifier`
`BytecodeArrayWriter` (`src/interpreter/bytecode-array-writer.cc`) consumes finalized `BytecodeNode`s and emits the raw byte stream into `ZoneVector<uint8_t> bytecodes_`:
- **Operand scaling**: Computes the minimum `OperandScale` (`kSingle` = 1 byte, `kDouble` = 2 bytes with `Wide` prefix, `kQuadruple` = 4 bytes with `ExtraWide` prefix) needed to encode all operands of a `BytecodeNode`.
- **Jump patching**: Forward jumps initially emit an 8-bit `Jump` placeholder and record an `UnresolvedJump` in `unresolved_jumps_`. When the target `BytecodeLabel` is bound (`BindLabel`), `PatchJump` patches the displacement if it fits in 8 bits or upgrades the instruction (when reserved), while backward jumps (`BindLoopHeader`) select 8-bit, 16-bit (`Wide`), or 32-bit (`ExtraWide`) jump encodings directly.
- **Dead-code and peephole elision**: Once a block-terminating instruction (`Return`, unconditional `Jump`, `Throw`, `ReThrow`, `Abort`) is emitted, `exit_seen_in_block_` is set and subsequent instructions are dropped until a label is bound or a try-handler starts. `MaybeElideLastBytecode` also overwrites a trailing side-effect-free accumulator load (such as `LdaTrue` or `LdaSmi`) if the very next instruction unconditionally clobbers the accumulator without reading it (provided neither instruction has a breakpoint-visible source position).
- **`BytecodeVerifier::Verify` (`src/sandbox/bytecode-verifier.h`)**: Immediately after `BytecodeArrayWriter::ToBytecodeArray` allocates the `BytecodeArray` in `TrustedSpace`, it invokes `BytecodeVerifier::Verify(isolate, bytecode_array, zone())` to protect V8's sandbox invariants:
  - `VerifyLight` (enabled by default via `--verify-bytecode-light`): Decodes the bytecode stream to build a bitmap of valid instruction start offsets (`valid_offsets`), verifies that every relative jump, constant-pool jump, and `SwitchOnSmiNoFeedback` table entry targets a valid instruction boundary, ensures the last instruction is a terminal bytecode, and checks that `HandlerTable` try-regions and context registers are in bounds.
  - `VerifyFull` (`--verify-bytecode-full`): Additionally validates every register, parameter, and `RegisterList` operand against `register_count` and `parameter_count`, forbids writes to fixed frame slots (`current_context`, `function_closure`) and any access to `bytecode_array` or `bytecode_offset`, validates constant pool indices, and checks runtime function IDs (`IsAllowedRuntimeFunction`).
  - Once verified, `bytecode->MarkVerified(isolate)` publishes the bytecode for execution.

---

## 2. Expression Result Scopes, Accumulator Preservation, and Register Allocation

### `ExpressionResultScope` Hierarchy (`src/interpreter/bytecode-generator.cc`)
In `BytecodeGenerator`, every AST expression visit executes inside an `ExpressionResultScope` (`execution_result()`). The scope dictates how the visited expression's value is returned and embeds a `RegisterAllocationScope` so temporary registers allocated while evaluating the expression are automatically freed when the scope exits.

| Scope Class | `BytecodeGenerator` Entry Points | `Kind` | Behavior |
|---|---|---|---|
| `EffectResultScope` | `VisitForEffect(expr)` | `kEffect` | Evaluates `expr` for side effects only; does not require the result to be materialized in the accumulator. |
| `ValueResultScope` | `VisitForAccumulatorValue(expr)`, `VisitForAccumulatorValueAsPropertyKey(expr)`, `VisitForRegisterValue(expr)` | `kValue` or `kValueAsPropertyKey` | Evaluates `expr` into the accumulator. Tracks a `TypeHint` (`kBoolean`, `kInternalizedString`, `kString`, `kAny`, `kUnknown`) and forwards it to `BytecodeRegisterOptimizer::SetTypeHintForAccumulator` on scope exit. |
| `TestResultScope` | `VisitForTest(expr, then_labels, else_labels, fallthrough)` | `kTest` | Evaluates `expr` as a boolean condition branching to `then_labels` or `else_labels` without materializing a boolean `true`/`false` in the accumulator. |

#### `TypeHint` Propagation
When an expression known to produce a specific type (for example, a comparison emitting `TestEqual`, or a string literal) executes inside a `ValueResultScope`, it calls `execution_result()->SetResultIsBoolean()` or `SetResultIsString()`. The `TypeHint` is stored on the accumulator's `RegisterInfo` in `BytecodeRegisterOptimizer` and propagated across register transfers (`Star`, `Ldar`, `Mov`). Downstream consumers query `ToBooleanModeFromTypeHint(type_hint)` to emit `JumpIfTrue` instead of `JumpIfToBooleanTrue`, or `ToBooleanLogicalNot` vs `LogicalNot`.

#### `TestResultScope` and `TestFallthrough`
`VisitForTest` constructs a `TestResultScope` holding `then_labels()`, `else_labels()`, and a `TestFallthrough` (`kThen`, `kElse`, or `kNone`):
- Logical operators (`VisitBinaryOperation` for `&&`/`||`, `VisitConditional`, `VisitCompareOperation`) inspect `execution_result()->IsTest()`, call `SetResultConsumedByTest()`, and emit conditional jumps directly to `then_labels()` / `else_labels()`.
- Unary `!` (`VisitNot`) in a test context simply calls `TestResultScope::InvertControlFlow()`, which swaps `then_labels_` and `else_labels_` and flips `fallthrough_` (`kThen <-> kElse`), then visits the sub-expression with zero extra instructions.
- If the visited AST node does not consume the test directly (`!result_consumed_by_test()`), `VisitForTest` lets the `TestResultScope` destructor run first (releasing temporary registers allocated during the expression) and then calls `BuildTest(ToBooleanModeFromTypeHint(type_hint), then_labels, else_labels, fallthrough)` to branch on the accumulator.

### `AccumulatorPreservingScope`
Because binary operations, property loads, and other sub-expressions clobber the accumulator, `AccumulatorPreservingScope` saves the accumulator across a sub-computation when `AccumulatorPreservingMode::kPreserve` is passed:
- Its constructor allocates a temporary register via `execution_result()->NewRegister()` and emits `builder()->StoreAccumulatorInRegister(reg)`.
- Its destructor restores the value via `builder()->LoadAccumulatorWithRegister(reg)`.

### `RegisterAllocationScope` and `BytecodeRegisterAllocator` (`src/interpreter/bytecode-register-allocator.h`)
Ignition's register file is partitioned into two regions:
1. **Fixed registers (`0 .. fixed_register_count - 1`)**: Allocated upfront by `BytecodeArrayBuilder` for local `Variable`s (and the context register). Their lifetimes span the entire function.
2. **Temporary registers (`>= fixed_register_count`)**: Bump-allocated dynamically by `BytecodeRegisterAllocator` (`next_register_index_`).

Key mechanics:
- **Scope-bound lifetimes**: Each `RegisterAllocationScope` (including the one embedded in every `ExpressionResultScope`) records `outer_next_register_index_ = allocator()->next_register_index()` on construction and calls `allocator()->ReleaseRegisters(outer_next_register_index_)` on destruction.
- **Contiguous `RegisterList` allocation**: Call, construct, and intrinsic bytecodes take an `OperandType::kRegList` operand (encoded as `(first_reg, reg_count)` in the bytecode, with `reg_count` often implicit in the opcode). `NewRegisterList(count)` and `GrowRegisterList(RegisterList*)` allocate contiguous register indices at the top of the bump allocator.
- **Avoiding fragmentation during argument evaluation**: When `BytecodeGenerator::VisitAndPushIntoRegisterList` evaluates an argument to append to a `RegisterList`, it first evaluates the argument inside a nested `ValueResultScope` (so any temporaries needed to compute the argument are allocated and freed when the `ValueResultScope` exits), and *only then* calls `allocator()->GrowRegisterList(reg_list)` and stores the accumulator into the newly grown slot.

---

## 3. `BytecodeRegisterOptimizer` and Register Equivalence Sets

`BytecodeRegisterOptimizer` (`src/interpreter/bytecode-register-optimizer.cc`) eliminates redundant `Ldar`, `Star`, and `Mov` instructions and avoids materializing temporary registers when the value already lives in a local or parameter register.

### Equivalence Set Representation
Every parameter, local, allocated temporary, and the virtual accumulator (`virtual_accumulator_`) is represented by a `RegisterInfo` object. Equivalence sets are maintained as circular doubly-linked lists of `RegisterInfo` nodes sharing an `equivalence_id_`. Each `RegisterInfo` tracks:
- `materialized_`: Whether a real instruction has actually written this register's current equivalence value.
- `allocated_`: Whether the register is currently live in `BytecodeRegisterAllocator`.
- `needs_flush_`: Whether the register is in `registers_needing_flushed_` and must be resets/materialized on `Flush()`.
- `type_hint_` and `variable_hint_`: Propagated across equivalents when transferring values.

### Lazy Transfer Semantics (`DoLdar`, `DoStar`, `DoMov`)
All three transfer methods delegate to `RegisterTransfer(input_info, output_info)`:
1. `output_info` is removed from its old equivalence set (materializing an alternate member first if `output_info` was the last materialized member of that set) and joined to `input_info`'s equivalence set via `AddToEquivalenceSet`.
2. **Temporary outputs are lazy**: If `output` is the accumulator or a temporary register (`!RegisterIsObservable(output)`), no bytecode is emitted (`output_info->set_materialized(false)`).
3. **Observable outputs are materialized**: If `output` is a local variable or parameter (`RegisterIsObservable(output)`, i.e. `reg != accumulator_ && reg < temporary_base_`), `OutputRegisterTransfer` immediately emits a `Star` or `Mov` into `output`. Furthermore, if `input` was also observable, `MarkTemporariesAsUnmaterialized` marks any temporaries in the set as unmaterialized so future reads prefer the local/parameter.
4. **Input operand rewriting (`GetInputRegister`)**: When a bytecode reads a register operand, `GetMaterializedEquivalentNotAccumulator` returns an already-materialized register in the same equivalence set (or materializes `input` from the accumulator if only the accumulator was materialized). Thus, `Ldar r0; Star tmp0; Add r1, [tmp0]` is rewritten directly to `Add r1, [r0]`, and `tmp0` is never written.
5. **Clobber protection (`PrepareOutputRegister` / `PrepareOutputRegisterList`)**: Before an instruction overwrites `reg` (or the accumulator), if `reg` is materialized and still needed by other allocated unmaterialized members in its equivalence set, `CreateMaterializedEquivalent` emits a `Star` or `Mov` to preserve the value in the lowest-indexed allocated equivalent (`GetEquivalentToMaterialize`).
6. **Contiguous `RegisterList` materialization (`PrepareInputRegisterList`)**: Because a `RegisterList` must occupy consecutive physical registers in the frame, every element in the list is forced to materialize into its own physical register via `Materialize(reg_info)` before the instruction executes.

### When Equivalence Sets Must Be Flushed (`Flush()`)
Equivalence sets are strictly basic-block-local and cannot survive control-flow joins, branches, or suspension points. `BytecodeRegisterOptimizer::Flush()` iterates `registers_needing_flushed_`, emits transfers (`OutputRegisterTransfer`) for any allocated unmaterialized registers, and resets every `RegisterInfo` into its own singleton equivalence set (also downgrading `VariableHintMode::kDefinitelyHasVariable` to `kMightHaveVariable`).

`Flush()` is called at four categories of boundaries:
1. **Control-flow and suspension instructions (`PrepareForBytecode`)**: Before emitting any bytecode where `Bytecodes::IsJump(bytecode)` is true, or before `SwitchOnSmiNoFeedback`, `SwitchOnGeneratorState`, `Debugger`, `SuspendGenerator`, `ResumeGenerator`, and `ForOfNext`.
2. **Binding control-flow targets (`BytecodeArrayBuilder::Bind`)**: When binding a `BytecodeLabel`, a `BytecodeLoopHeader`, or a `BytecodeJumpTable` case label, so predecessors cannot leak equivalence assumptions into a merge block.
3. **Exception handler boundaries (`BytecodeArrayBuilder::MarkTryBegin`)**: At the start of a `try` block so registers read by a `catch` or `finally` handler are guaranteed to be materialized.
4. **End of compilation (`BytecodeArrayBuilder::ToBytecodeArray`)**: Before finalizing the `BytecodeArrayWriter`.

---

## 4. Register File and Stack Frame Layout

Ignition is a **register machine with an implicit accumulator**:
- **The Accumulator**: An implicit register held in a dedicated machine register during handler execution (`kInterpreterAccumulatorRegister`, e.g. `rax` on x64, `x0` on arm64). Most bytecodes read one input from or write their output to the accumulator, keeping instructions 1-2 bytes shorter than 3-address formats.
- **Frame Registers (`r0, r1, ...`) and Parameters (`a0, a1, ...`)**: Stored as stack slots in the `InterpretedFrame` (`src/execution/frame-constants.h`), addressed via negative and positive offsets from the frame pointer (`fp` / `rbp` / `x29`).

```
High Addresses (Caller Frame)
+-----------------------------------+
| Parameter a_{n-1}                 |  fp + kFirstParamFromFp + (n-1)*kSystemPointerSize
| ...                               |
| Parameter a0                      |  fp + kCallerSpOffset + kSystemPointerSize
| Receiver (this)                   |  fp + kCallerSpOffset
+-----------------------------------+
| Return Address (saved PC)         |  fp + kCallerPCOffset
| Saved Frame Pointer (caller FP)   |  fp  <-- Frame Pointer (kInterpreterRegisterFileRegister)
+-----------------------------------+
| Context (current_context)         |  fp - 1 * kSystemPointerSize  (Register::current_context())
| Closure (function_closure)        |  fp - 2 * kSystemPointerSize  (Register::function_closure())
| Argument Count (argc)             |  fp - 3 * kSystemPointerSize  (StandardFrameConstants::kArgCOffset, Register::argument_count())
| BytecodeArray (bytecode_array)    |  fp - 4 * kSystemPointerSize  (UnoptimizedFrameConstants::kBytecodeArrayFromFp, Register::bytecode_array())
| BytecodeOffset / FeedbackCell     |  fp - 5 * kSystemPointerSize  (UnoptimizedFrameConstants::kBytecodeOffsetOrFeedbackCellFromFp, Register::bytecode_offset())
| FeedbackVector (feedback_vector)  |  fp - 6 * kSystemPointerSize  (UnoptimizedFrameConstants::kFeedbackVectorFromFp, Register::feedback_vector())
+-----------------------------------+
| Register r0 (fixed local 0)       |  fp - 7 * kSystemPointerSize  (UnoptimizedFrameConstants::kRegisterFileFromFp)
| Register r1 (fixed local 1)       |  fp - 8 * kSystemPointerSize
| ...                               |
| Register r_{fixed-1}              |
| Register r_{fixed} (temporary 0)  |
| ...                               |  <-- Stack Pointer (sp)
Low Addresses
```

Register operands in bytecode encode a signed offset relative to `kRegisterFileFromFp` (`Register::ToOperand()` / `Register::FromOperand()`), allowing the same operand encoding to address local/temporary registers (`r0..`), fixed frame header slots (`current_context`, `function_closure`), and incoming arguments (`a0..`, `<this>`).

---

## 5. Bytecode Format, Wide/ExtraWide Scaling, and Short Star Lookahead

### Operand Scaling and the 768-Entry Dispatch Table
Each bytecode starts with a 1-byte opcode (`src/interpreter/bytecodes.h`), followed by zero or more operands whose types (`OperandType`) and widths (`OperandSize`) are defined in `BYTECODE_LIST`.

To keep common instructions compact while supporting functions with more than 255 locals or constants, Ignition supports three operand scales (`OperandScale`):
- **`OperandScale::kSingle` (scale index 0)**: Default; no prefix byte. Register, index, flag, and immediate operands are 1 byte (`OperandSize::kByte`).
- **`OperandScale::kDouble` (scale index 1)**: Prefixed with `Bytecode::kWide` (`0x00`). Scalable operands widen to 2 bytes (`OperandSize::kShort`, `uint16_t` / `int16_t`).
- **`OperandScale::kQuadruple` (scale index 2)**: Prefixed with `Bytecode::kExtraWide` (`0x01`). Scalable operands widen to 4 bytes (`OperandSize::kQuad`, `uint32_t` / `int32_t`).

The interpreter's `dispatch_table_` (`Interpreter::kDispatchTableSize = 3 * 256 = 768` entries in `src/interpreter/interpreter.h`) is indexed by `(scale_index * 256) + static_cast<size_t>(bytecode)`:
- Entries `0..255`: `OperandScale::kSingle` handlers.
- Entries `256..511`: `OperandScale::kDouble` (`Wide`) handlers.
- Entries `512..767`: `OperandScale::kQuadruple` (`ExtraWide`) handlers.

#### `DispatchWide` and the `SaveBytecodeOffset()` `- 1` Adjustment
When the `Wide` (`0x00`) or `ExtraWide` (`0x01`) handler runs (`InterpreterAssembler::DispatchWide(OperandScale)` in `src/interpreter/interpreter-assembler.cc`):
1. It increments `kInterpreterBytecodeOffsetRegister` (`BytecodeOffset()`) by `1` so that it points to the actual opcode byte immediately following the prefix byte.
2. It loads `next_bytecode = LoadUint8(BytecodeArray(), next_bytecode_offset)` and jumps to `dispatch_table[(scale_index << 8) + next_bytecode]`.

Because a wide handler receives `BytecodeOffset()` pointing at the opcode byte (1 byte *past* the `Wide`/`ExtraWide` prefix), any runtime call, IC call, or stack-frame update inside a handler that needs the true instruction start offset must adjust for the prefix:
- `InterpreterAssembler::SaveBytecodeOffset()` checks `if (operand_scale() != OperandScale::kSingle)` and subtracts `1` (`IntPtrSub(BytecodeOffset(), IntPtrConstant(1))`) before Smi-tagging and storing the offset into `InterpretedFrame::bytecode_offset` (`Register::bytecode_offset()`).
- Conversely, `InterpreterAssembler::ReloadBytecodeOffset()` loads and untags `Register::bytecode_offset()` from the frame and, if `operand_scale() != OperandScale::kSingle`, adds `1` back so relative operand offsets inside the wide handler remain aligned with `OperandScale::kSingle`.

### `Star0..Star15` Shortcodes and `StarDispatchLookahead` (`InlineShortStar`)
Storing the accumulator into one of the first 16 registers (`r0..r15`) is the single most frequent operation in Ignition bytecode. To avoid both the 1-byte register operand and an extra indirect branch:
1. **Single-byte `Star0..Star15` opcodes**: `SHORT_STAR_BYTECODE_LIST` (`src/interpreter/bytecodes.h`) defines 16 single-byte opcodes (`Star15` down to `Star0`, placed contiguously right before the final `Bytecode::kIllegal` opcode so `bytecode >= Bytecode::kFirstShortStar` is a single unsigned comparison). In the builtins table, all 16 opcodes share a single builtin handler (`Star0` / `Star0Assembler`, mapped via `BuiltinIndexFromBytecode` in `src/builtins/builtins.h`), which computes the target register slot directly from the opcode byte (`InterpreterAssembler::StoreRegisterForShortStar`).
2. **Inlined `StarDispatchLookahead`**: For frequent accumulator-producing bytecodes where `Bytecodes::IsStarLookahead(bytecode, operand_scale)` is true (such as `LdaNamedProperty`, `LdaKeyedProperty`, `LdaGlobal`, `LdaTheHole`, `LdaZero`, `LdaSmi`, `Add`, `Sub`, etc. at `OperandScale::kSingle`), `InterpreterAssembler::DispatchToBytecodeWithOptionalStarLookahead` calls `StarDispatchLookahead(target_bytecode)` before dispatching:
   - The handler checks `UintPtrGreaterThanOrEqual(target_bytecode, IntPtrConstant(Bytecode::kFirstShortStar))`.
   - If the next bytecode is indeed `Star0..Star15`, `InlineShortStar(target_bytecode)` executes the store into `r0..r15` *inline inside the current handler*, advances `BytecodeOffset()` by 1, loads the following opcode byte, and dispatches to it via a second indirect jump.
   - This eliminates an entire indirect branch and separates the branch predictor site for `Load/Op -> StarN -> NextOp` from `Load/Op -> OtherOp`.

---

## 6. Bytecode Handler Compilation: `InterpreterAssembler` and Migration to TSA

Bytecode handlers are generated during `mksnapshot` (`Interpreter::Initialize` in `src/interpreter/interpreter.cc` and `SetupIsolateDelegate::SetupBuiltinsInternal` in `src/builtins/setup-builtins-internal.cc`) and embedded into the V8 binary (`EmbeddedData`).

### Standard CSA Bytecode Handlers (`IGNITION_HANDLER` / `BCH`)
Most bytecode handlers are defined in `src/interpreter/interpreter-generator.cc` using the `IGNITION_HANDLER(Name, InterpreterAssembler)` macro:
- `InterpreterAssembler` (`src/interpreter/interpreter-assembler.h`) subclasses `CodeStubAssembler` and adds bytecode-specific primitives: `GetAccumulator()`, `SetAccumulator()`, `BytecodeOperandReg(index)`, `BytecodeOperandIdx(index)`, `BytecodeOperandUImm(index)`, `Dispatch()`, `Jump(delta)`, and `UpdateInterruptBudget(weight)`.
- Each handler has `InterpreterDispatchDescriptor` linkage, keeping five critical state values pinned in machine registers across tail-call dispatches:
  - `kInterpreterAccumulatorRegister`: Current accumulator value.
  - `kInterpreterBytecodeOffsetRegister`: Current byte offset within the `BytecodeArray`.
  - `kInterpreterBytecodeArrayRegister`: Pointer to the active `BytecodeArray` (in `TrustedSpace`).
  - `kInterpreterDispatchTableRegister`: Base address of `Isolate::interpreter_dispatch_table()`.
  - `kInterpreterRegisterFileRegister`: Frame pointer (`fp`) used to address `r0..rN` and `a0..aM`.
- **Turboshaft Backend (`CodeAssemblerTurboshaftCompilationJob`)**: Although `InterpreterAssembler` builds a `CodeAssembler` / `RawMachineAssembler` graph, bytecode handlers are **not** compiled by the legacy TurboFan backend. `Pipeline::NewBytecodeHandlerCompilationJob` (`src/compiler/pipeline.cc`) returns a `CodeAssemblerTurboshaftCompilationJob` (`TurboshaftPipelineKind::kCSA`), which translates the graph to Turboshaft IR (`CreateGraphFromTurbofan`) and optimizes it through `BuiltinPipeline::OptimizeBuiltin()`.

### Direct Turboshaft Bytecode Handlers (`V_TSA` / `BCH_TSA`)
V8 is migrating bytecode handlers to **TSA (`TurboshaftBuiltinsAssembler`)** to emit Turboshaft IR directly without going through `CodeStubAssembler` or `RawMachineAssembler`:
- Bytecodes migrated to TSA are declared with `V_TSA` in `BYTECODE_LIST_WITH_UNIQUE_HANDLERS_IMPL` (`src/interpreter/bytecodes.h`, gated by `V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS`, e.g. `BitwiseNot`).
- They generate `BCH_TSA` builtins compiled via `CompileBytecodeHandlerTSA` -> `Pipeline::NewBytecodeHandlerCompilationJobTSA` (`src/compiler/turboshaft/builtin-compiler.cc`, `TurboshaftPipelineKind::kTSABuiltin`), invoking `GenerateBytecodeHandlerTSA` (`BytecodeHandlerBuiltinsFromDSLAssembler`, combining `InterpreterBuiltinsAssembler` and `TurboshaftBuiltinsAssembler`).

---

## 7. Interrupt Budget, Feedback Vectors, and Tier-Up / OSR

Ignition drives V8's adaptive tiering pipeline (`Ignition -> Sparkplug -> Maglev -> Turbofan/Turboshaft`):
- **Feedback Vector Slots**: Stateful bytecodes (`GetNamedProperty`, `SetNamedProperty`, `CallProperty`, `Add`, `Compare`, etc.) carry an embedded feedback slot index (`OperandType::kIdx`). Handlers pass the `FeedbackVector` (cached on the `ClosureFeedbackCellArray` / `JSFunction`) and slot index to IC builtins (`LoadIC_BytecodeHandler`, `BinaryOp_Baseline`, etc.).
- **Interrupt Budget**: Every backward jump (`JumpLoop`) and function return (`Return`) calls `InterpreterAssembler::UpdateInterruptBudget`:
  - `BytecodeArray::interrupt_budget` is decremented by the number of bytecode bytes executed since the last budget check.
  - Backward jumps (`JumpLoop`) also check the stack limit (`StackCheck`) so tight loops can process interrupts, GC requests, and On-Stack Replacement (OSR) triggers (`Runtime::kBytecodeBudgetInterruptWithStackCheck_Ignition` vs `Runtime::kBytecodeBudgetInterrupt_Ignition`).

---

## 8. Key Files Reference

| File | Purpose |
|---|---|
| `src/interpreter/bytecodes.h` | Master `BYTECODE_LIST` macro defining all opcodes, `OperandType`s, `AccumulatorUse`, `SHORT_STAR_BYTECODE_LIST`, and `V_TSA` handlers. |
| `src/interpreter/bytecode-operands.h` | `OperandScale` (`kSingle`, `kDouble`, `kQuadruple`), `OperandSize`, `OperandType`, and `ImplicitRegisterUse`. |
| `src/interpreter/bytecode-generator.{h,cc}` | `BytecodeGenerator` (`AstVisitor`), `ExpressionResultScope` (`EffectResultScope`, `ValueResultScope`, `TestResultScope`), `AccumulatorPreservingScope`, and `RegisterAllocationScope`. |
| `src/interpreter/bytecode-array-builder.{h,cc}` | High-level bytecode emission API, operand translation, jump label binding, and constant pool reservation. |
| `src/interpreter/bytecode-register-allocator.h` | `BytecodeRegisterAllocator` bump allocator for temporary `Register`s and contiguous `RegisterList`s. |
| `src/interpreter/bytecode-register-optimizer.{h,cc}` | Lazy equivalence-set register optimizer (`DoLdar`, `DoStar`, `DoMov`, `Flush`). |
| `src/interpreter/bytecode-array-writer.{h,cc}` | Byte-level encoding, `Wide`/`ExtraWide` scaling, jump patching, and dead-code elision. |
| `src/sandbox/bytecode-verifier.{h,cc}` | `BytecodeVerifier::Verify` (`VerifyLight` and `VerifyFull`) validating `BytecodeArray` invariants in `TrustedSpace`. |
| `src/interpreter/interpreter.{h,cc}` | `Interpreter` facade, `NewCompilationJob`, and the 768-entry `dispatch_table_`. |
| `src/interpreter/interpreter-assembler.{h,cc}` | `InterpreterAssembler` (`CodeStubAssembler` subclass) implementing operand decoding, `SaveBytecodeOffset`, `DispatchWide`, and `StarDispatchLookahead`. |
| `src/interpreter/interpreter-generator.{h,cc}` | `IGNITION_HANDLER` definitions for all Ignition bytecodes (and experimental TSA handlers). |
