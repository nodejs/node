# Assembler Architecture (`Assembler` and `MacroAssembler`)

The Assembler subsystem (`src/codegen/`) is the lowest layer of code generation in V8. Every execution tier that emits native machine code (Ignition/CSA/TSA builtins, Sparkplug, Maglev, Turboshaft/Turbofan, Wasm Liftoff, and RegExp native codegen) funnels through `Assembler` and `MacroAssembler` to produce an executable `InstructionStream` and `Code` object.

---

## 1. Three-Tier Class Hierarchy

V8 separates low-level instruction encoding from V8-specific execution conventions through a three-tier inheritance chain:

```
AssemblerBase                (src/codegen/assembler.h)
   │  Architecture-independent buffer management, AssemblerOptions,
   │  JumpOptimizationInfo, and RelocInfo writer state.
   ▼
Assembler                    (src/codegen/<arch>/assembler-<arch>.{h,cc})
   │  1-to-1 ISA instruction encoding, branch label linking/patching,
   │  and constant/veneer pool emission (BlockPoolsScope).
   ▼
MacroAssemblerBase           (src/codegen/macro-assembler-base.{h,cc})
   │  Cross-platform Isolate/Root-relative helpers (LoadRoot, CanBeImmediate),
   │  CodeSandboxingMode tracking, and indirect pointer table helpers.
   ▼
MacroAssembler               (src/codegen/<arch>/macro-assembler-<arch>.{h,cc})
      Architecture-specific pseudo-instructions, V8 ABI transitions, stack
      frames, pointer decompression, write barriers, and CFI landing pads.
```

### Tier 1: `AssemblerBase` (`src/codegen/assembler.h`)
Manages the underlying `AssemblerBuffer` (`buffer_`, defaulting to a 4 KB dynamically growing buffer or an externally provided buffer), tracks the write cursor `pc_` (`pc_offset()`), holds `AssemblerOptions` (such as `builtin_call_jump_mode`, `enable_simulator_code`, `is_wasm`), and stores optional `CodeCommentsWriter` and `JumpOptimizationInfo` state.

### Tier 2: `Assembler` (`src/codegen/<arch>/assembler-<arch>.h`)
Implements exact 1-to-1 instruction encoding for the target CPU architecture (e.g., `movq`, `addl`, `j(Condition, Label*)` on x64; `ldr`, `add`, `b(Label*, Condition)` on arm64).
- Does **not** know about V8 heap layouts, frames, or calling conventions.
- Handles `Label` binding and branch displacement patching.
- Manages inline constant pools and branch veneer pools on fixed-width RISC architectures.

### Tier 3: `MacroAssemblerBase` and `MacroAssembler`
`MacroAssemblerBase` (`src/codegen/macro-assembler-base.h`) inherits from `Assembler` and adds cross-architecture Isolate, RootTable, and V8 Sandbox state. `MacroAssembler` (`src/codegen/<arch>/macro-assembler-<arch>.h`, aliased as `masm`) adds higher-level V8 operations:
- **Pointer Compression & Sandbox**: `DecompressTagged`, `DecompressTaggedSigned`, `LoadTrustedPointerField` (resolving a `TrustedPointerHandle` through the Isolate's `TrustedPointerTable`), `LoadCodeInstructionStart`, and `LoadExternalPointerField` (resolving an `ExternalPointerHandle` through the `ExternalPointerTable`).
- **Frame Management**: `EnterFrame(StackFrame::Type)`, `LeaveFrame(StackFrame::Type)`, `Prologue()`, `StubPrologue(...)`.
- **ABI & Calls**: `CallRuntime`, `TailCallRuntime`, `CallBuiltin`, `TailCallBuiltin`, `PrepareCallCFunction`, and `CallCFunction`.
- **GC Write Barriers**: `RecordWriteField` and `RecordWrite`.

---

## 2. Root-Relative Addressing and Static Roots (`V8_STATIC_ROOTS_BOOL`)

To avoid embedding absolute 64-bit Isolate addresses into generated code (which would prevent sharing read-only builtin code across Isolates and processes), V8 pins a dedicated machine register (`kRootRegister`) to point into the current `IsolateData` (`Isolate::isolate_root()`, biased by `IsolateData::kRootRegisterBias`).

### Dedicated Registers Across Primary Architectures

| Register Role | C++ Constant | x64 (`register-x64.h`) | arm64 (`register-arm64.h`) | ia32 (`register-ia32.h`) | arm (`register-arm.h`) |
|---|---|---|---|---|---|
| **Root Register** | `kRootRegister` | `r13` | `x26` (`rr`) | `ebx` (when available) | `r10` |
| **Pointer Compression Cage Base** | `kPtrComprCageBaseRegister` | `r14` | `x28` (`cp` when enabled) | N/A | N/A |
| **JS Context Register** | `kContextRegister` | `rsi` | `x27` (`cp`) | `esi` | `r7` |

### Root-Relative Addressing (`MacroAssemblerBase`)
Because `IsolateData` lays out the `RootsTable`, the builtin code pointer table (`builtin_tier0_entry_table` / `builtin_entry_table`), external reference table, and thread-local top at fixed compile-time offsets from `Isolate::isolate_root()`, `MacroAssemblerBase` can load or store any Isolate-global slot with a single `kRootRegister`-relative memory operand:
- `MacroAssemblerBase::RootRegisterOffsetForRootIndex(RootIndex)`: Computes the signed byte offset from `kRootRegister` to a `RootsTable` entry.
- `MacroAssemblerBase::RootRegisterOffsetForBuiltin(Builtin)`: Computes the offset to a builtin entry pointer in `IsolateData`.
- `MacroAssemblerBase::RootRegisterOffsetForExternalReference(Isolate*, ExternalReference)`: Computes the offset into the Isolate's `external_reference_table`.
- `LoadRoot(Register destination, RootIndex index)` / `LoadRootRelative(Register destination, int32_t offset)` / `StoreRootRelative(int32_t offset, Register value)`: Emit `[kRootRegister + offset]` memory operations.

### Compile-Time Static Roots (`V8_STATIC_ROOTS_BOOL`)
When pointer compression and static roots are enabled (`V8_STATIC_ROOTS_BOOL` in `include/v8-internal.h`), read-only heap roots (`RootsTable::IsReadOnly(RootIndex)`, such as `undefined_value`, `null_value`, `true_value`, `false_value`, `the_hole_value`, and core `Map`s) are placed at fixed, deterministic 32-bit compressed offsets in `ReadOnlySpace` (`StaticReadOnlyRoot` in `src/roots/static-roots.h`).

`MacroAssemblerBase::CanBeImmediate(RootIndex index)` (`src/codegen/macro-assembler-base.h`) returns:
```cpp
bool MacroAssemblerBase::CanBeImmediate(RootIndex index) {
  return V8_STATIC_ROOTS_BOOL && RootsTable::IsReadOnly(index);
}
```
When `CanBeImmediate(index)` is true, `MacroAssemblerBase::ReadOnlyRootPtr(index)` returns the compile-time 32-bit compressed `Tagged_t` constant:
- Comparisons against read-only roots (`CompareRoot`, `JumpIfRoot`, `JumpIfNotRoot`) emit a single 32-bit immediate compare instruction (`cmpl reg, #imm32` on x64) instead of a 64-bit load from `[kRootRegister + offset]` or decompressing the pointer.
- Storing a read-only root into a compressed heap field emits an immediate store without occupying a scratch register.

---

## 3. Hardware Sandboxing Mode (`CodeSandboxingMode`) and CFI Landing Pads

### `CodeSandboxingMode` (`kSandboxed` vs `kUnsandboxed`)
Under V8's hardware-assisted sandbox model (`V8_ENABLE_SANDBOX_HARDWARE_SUPPORT`), generated code executes in one of two modes tracked on `MacroAssemblerBase::sandboxing_mode_` (`src/objects/code-kind.h`):
- **`CodeSandboxingMode::kSandboxed`** (default): Code runs with sandbox memory restrictions active (e.g. via Memory Protection Keys / PKEYs) so untrusted/in-sandbox memory accesses are confined.
- **`CodeSandboxingMode::kUnsandboxed`**: Code runs with full out-of-sandbox write privileges (used by specific trusted trampolines and runtime transition stubs).

`MacroAssemblerBase::set_builtin(Builtin)` automatically sets `sandboxing_mode_` to `Builtins::SandboxingModeOf(builtin)` (`src/codegen/macro-assembler-base.h`). `MacroAssembler` enforces mode transitions at entry/exit boundaries:
- For example, `MacroAssembler::ExceptionHandler()` (`src/codegen/x64/macro-assembler-x64.cc`) emits a `CodeEntry()` landing pad, asserts `AssertInSandboxedExecutionMode()` (because exception unwinding always dispatches in sandboxed mode), and if the handler belongs to a `CodeSandboxingMode::kUnsandboxed` builtin, immediately emits `ExitSandbox()`.
- `SetSandboxingModeForCurrentBuiltin()` switches hardware execution state upon entering a builtin whose `CodeSandboxingMode` differs from its caller.

### Control-Flow Integrity Landing Pads (`CodeEntry()`)
To support hardware forward-edge Control-Flow Integrity (Intel CET / Indirect Branch Tracking on x64, and Armv8.5-A Branch Target Identification / BTI on arm64), `MacroAssembler::CodeEntry()` emits an architectural landing-pad instruction at every indirect call or jump target:
- **x64**: `endbr64` (via `Assembler::endbr64()`).
- **arm64**: `bti c` (or `bti jc` for jump/call targets via `Assembler::bti(BranchTargetIdentifier)` / `MacroAssembler::CallTarget()` / `JumpTarget()`).

Whenever you add a new builtin entry point, bytecode handler prologue, exception handler landing pad (`ExceptionHandler()`), or jump-table target that can be reached via an indirect branch (`call reg` / `br xN`), you must emit `CodeEntry()` (or `JumpTarget()`/`CallTarget()` on arm64) as the first instruction at that label.

---

## 4. Labels, Constant/Veneer Pools (`BlockPoolsScope`), and Two-Pass Jump Optimization

### `Label` Linking and Patching (`src/codegen/label.h`)
A `Label` exists in one of three states:
1. **Unlinked (`is_unused()`)**: Not yet referenced or bound.
2. **Linked (`is_linked()` / `is_near_linked()`)**: Forward-referenced by one or more branch instructions, but not yet bound. To avoid heap allocations while building code, the `Assembler` embeds an intrusive singly-linked list of unresolved branch offsets directly inside the displacement fields of the forward branch instructions in `buffer_` (`Label::pos()` points to the most recent unresolved branch).
3. **Bound (`is_bound()`)**: `masm->bind(&label)` has been called. Binding walks the intrusive chain of forward references, patches each branch instruction with the relative offset to the current `pc_offset()`, and records the final bound offset in `label` for future backward jumps.

### Constant Pools and Branch Veneer Pools (`BlockPoolsScope`)
On RISC architectures with fixed 32-bit instruction encodings (such as `arm64`, `arm`, `riscv64`, and `loong64`), 64-bit immediates cannot be encoded inline in a single instruction, and conditional branches have limited PC-relative range (e.g., `+-1 MB` for `B.cond` and `+-32 KB` for `TBZ`/`TBNZ` on arm64):
- **Constant Pools (`ConstantPool` in `src/codegen/constant-pool.h`)**: Literal loads (`ldr(reg, Imm)` / `RelocInfo`) record entries in a pending pool and use PC-relative `ldr` instructions. The `Assembler` periodically emits the constant pool inline into the instruction stream (jumping around it if emitted in reachable code) before the PC-relative offset from the earliest pending load exceeds its architectural range.
- **Veneer Pools (`CheckVeneerPool` / `EmitVeneers` in `src/codegen/arm64/assembler-arm64.h`)**: Unresolved forward conditional branches are tracked in `unresolved_branches_` keyed by their maximum reachable PC offset (`MaxPcOffset`). Before an unresolved branch goes out of range, the `Assembler` emits a **veneer** (an unconditional `b` instruction with a `+-128 MB` range) and chains the short conditional branch through the veneer.
- **`Assembler::BlockPoolsScope` (and `BlockConstPoolScope` / `BlockVeneerPoolScope`)**: Some code sequences rely on fixed instruction offsets between adjacent instructions (for example, jump table offset calculations, patchable inline caches, or instruction sequences following a ` bl ` where the return address is used to locate inline metadata). Entering an `Assembler::BlockPoolsScope(masm, margin)` first flushes any pending constant or veneer pools that might overflow within `margin` bytes, then increments `const_pool_blocked_nesting_` and `veneer_pool_blocked_nesting_` so the `Assembler` is forbidden from injecting pools inside the scope.

### Two-Pass Jump Optimization (`JumpOptimizationInfo`)
On x64 and ia32, forward conditional and unconditional jumps whose targets are not yet bound must conservatively reserve a 32-bit displacement (`near_jump` = 5-6 bytes) unless the caller explicitly passes `Label::kNear` (8-bit displacement = 2 bytes).

During `mksnapshot` (and when `--turbo-rewrite-far-jumps` is enabled), `turboshaft::Pipeline::GenerateCode` (`src/compiler/turboshaft/pipelines.cc`) uses `JumpOptimizationInfo` (`src/codegen/assembler.h`) to shrink eligible 32-bit forward jumps into 8-bit short jumps in two passes:
1. **Collection Pass (`JumpOptimizationInfo::is_collecting()`)**: Code generation runs normally. Every unbound forward jump records its position and instruction size in `farjmps` (`may_optimizable_farjmp`). When each target `Label` is bound, if the actual distance fits in an 8-bit signed displacement (accounting for alignment directives tracked in `align_pos_size`), the jump is moved into `optimizable_farjmps` (`optimizable = true`). The pass also computes `hash_code` over the generated instructions.
2. **Optimization Pass (`JumpOptimizationInfo::is_optimizing()`)**: If `jump_opt.is_optimizable()` is true, `PipelineData::ClearCodegenComponent()` discards the first pass's codegen output, calls `jump_opt.set_optimizing()`, and re-runs instruction selection, register allocation, and code assembly. During this second pass, the `Assembler` consults `optimizable_farjmps` and emits 2-byte short jumps directly. `VerifyGeneratedCodeIsIdempotent()` asserts that the instruction stream prior to code assembly produced the exact same `hash_code` in both passes.

---

## 5. RelocInfo, Safepoints, and Deoptimization Metadata

Because the `InstructionStream` object can be relocated or serialized into the snapshot and contains references to GC-managed heap objects, the `Assembler` records side-table metadata alongside the machine instructions:
- **`RelocInfo` (`src/codegen/reloc-info.h`)**: Written backwards from the end of the `AssemblerBuffer` by `RelocInfoWriter` while machine instructions grow forwards from the start. Tags include `CODE_TARGET`, `RELATIVE_CODE_TARGET`, `COMPRESSED_EMBEDDED_OBJECT`, `FULL_EMBEDDED_OBJECT`, `EXTERNAL_REFERENCE`, `INTERNAL_REFERENCE`, `WASM_CALL`, `WASM_STUB_CALL`, and `WASM_CANONICAL_SIG_ID`.
- **`SafepointTableBuilder` (`src/codegen/safepoint-table.h`)**: Records every return address (`pc_offset`) where a GC can occur while this frame is on the stack, along with a bitmap of tagged stack slots and registers that the GC must visit and update.
- **`HandlerTable` (`src/codegen/handler-table.h`)**: Maps try-range PC offsets to catch/finally landing-pad offsets (marked with `masm->ExceptionHandler()`).

---

## 6. Key Files Reference

| File | Purpose |
|---|---|
| `src/codegen/assembler.h` | `AssemblerBase`, `AssemblerOptions`, `JumpOptimizationInfo`, and `AssemblerBuffer`. |
| `src/codegen/macro-assembler-base.{h,cc}` | `MacroAssemblerBase`, root-relative addressing (`LoadRoot`, `CanBeImmediate`, `ReadOnlyRootPtr`), and `CodeSandboxingMode`. |
| `src/codegen/macro-assembler.h` | Includes the target architecture's `MacroAssembler` and `FrameScope`. |
| `src/codegen/<arch>/assembler-<arch>.{h,cc}` | ISA-level instruction encoding, `Label` patching, and `BlockPoolsScope` / constant & veneer pools (`x64`, `arm64`, `ia32`, `arm`). |
| `src/codegen/<arch>/macro-assembler-<arch>.{h,cc}` | High-level V8 macro instructions, pointer decompression, sandbox helpers, `CodeEntry()`, and ABI transitions. |
| `src/codegen/<arch>/register-<arch>.h` | Machine register definitions and fixed V8 register assignments (`kRootRegister`, `kPtrComprCageBaseRegister`, `kContextRegister`). |
| `src/codegen/label.h` | `Label` state machine and intrusive forward-branch linking. |
| `src/codegen/constant-pool.{h,cc}` | Inline constant pool management for RISC architectures. |
| `src/codegen/reloc-info.{h,cc}` | `RelocInfo`, `RelocInfoWriter`, and `RelocIterator` for GC and snapshot relocation. |
| `src/codegen/interface-descriptors.h` | `CallInterfaceDescriptor` definitions specifying register and stack calling conventions for stubs and builtins. |
