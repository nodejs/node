# CodeStubAssembler (CSA) and TurboshaftBuiltinsAssembler (TSA)

`CodeStubAssembler` (CSA) and its successor `TurboshaftBuiltinsAssembler` (TSA) are V8's portable C++ domain-specific assemblers for writing performance-critical builtins, Inline Caches (ICs), and Ignition bytecode handlers. They allow engineers (and the Torque compiler) to write low-level, statically-typed control-flow and memory operations once in C++ and compile them to optimized machine code for all target architectures (`x64`, `arm64`, `ia32`, `arm`, etc.) during `mksnapshot`.

---

## 1. Compilation Pipeline: The Turboshaft CSA Backend

All `CodeStubAssembler` code (including C++ CSA builtins, Torque-generated CSA code, and `InterpreterAssembler` bytecode handlers) is compiled through **Turboshaft**, not the legacy TurboFan backend.

When `Pipeline::NewCSLinkageCodeStubBuiltinCompilationJob`, `Pipeline::NewJSLinkageCodeStubBuiltinCompilationJob`, or `Pipeline::NewBytecodeHandlerCompilationJob` (`src/compiler/pipeline.cc`) runs during `mksnapshot`, it creates a `CodeAssemblerTurboshaftCompilationJob`:

```
CodeStubAssembler / Torque-generated *FromDSLAssembler
   │  (src/codegen/code-stub-assembler.{h,cc})
   ▼
CodeAssembler                 (src/compiler/code-assembler.{h,cc})
   │  Wraps RawMachineAssembler to build a scheduled sea-of-nodes machine graph
   │  with TNode<T>, TVariable<T>, and Label control-flow blocks.
   ▼
CreateGraphFromTurbofan       (src/compiler/turboshaft/phase.h)
   │  Translates the RawMachineAssembler schedule into a Turboshaft Graph
   │  (TurboshaftPipelineKind::kCSA).
   ▼
BuiltinPipeline::OptimizeBuiltin()  (src/compiler/turboshaft/pipelines.cc)
   │  1. CsaEarlyMachineOptimizationPhase  (MachineOptimizationReducer + ValueNumberingReducer)
   │  2. CsaLoadEliminationPhase           (LoadStoreSimplification + LateLoadElimination)
   │  3. CsaLateEscapeAnalysisPhase        (LateEscapeAnalysisReducer + MachineOptimization)
   │  4. CsaBranchEliminationPhase         (BranchEliminationReducer + MachineOptimization)
   │  5. CsaEffectsComputationPhase        (optional: computes BuiltinEffects for JS-to-Builtin calls)
   │  6. CsaOptimizePhase / CsaMemoryOptimizationPhase (Pretenuring + MemoryOptimizationReducer)
   │  7. CodeEliminationAndSimplificationPhase (DeadCodeEliminationReducer)
   ▼
Turboshaft Backend            (src/compiler/turboshaft/pipelines.cc)
      InstructionSelectionPhase -> RegisterAllocation -> AssembleCode -> Code
```

### Key CSA Optimization Phases (`src/compiler/turboshaft/csa-optimize-phase.{h,cc}`)
- **`CsaEarlyMachineOptimizationPhase`**: Constant folding, strength reduction, and global value numbering (`MachineOptimizationReducer`, `ValueNumberingReducer`).
- **`CsaLoadEliminationPhase`**: Normalizes addressing modes (`LoadStoreSimplificationReducer`), eliminates redundant heap loads across basic blocks (`LateLoadEliminationReducer`), and cleans up the resulting arithmetic.
- **`CsaLateEscapeAnalysisPhase`**: Eliminates non-escaping allocations (`LateEscapeAnalysisReducer`).
- **`CsaBranchEliminationPhase`**: Folds redundant branches and diamond merges (`BranchEliminationReducer`, `RequiredOptimizationReducer`).
- **`CsaOptimizePhase` / `CsaMemoryOptimizationPhase`**: Folds sequential bump-pointer allocations into single allocations, optimizes write barriers (`MemoryOptimizationReducer`, `PretenuringPropagationReducer`), and runs `CodeEliminationAndSimplificationPhase` (`DeadCodeEliminationReducer`) before instruction selection.

---

## 2. `TurboshaftBuiltinsAssembler` (TSA): Direct Turboshaft Builtin Generation

While CSA still constructs an intermediate `RawMachineAssembler` graph before converting it via `CreateGraphFromTurbofan`, V8 has introduced **TSA (`TurboshaftBuiltinsAssembler`)** in `src/codegen/turboshaft-builtins-assembler-inl.h` to emit Turboshaft IR directly.

### How TSA Works
- **Direct Turboshaft Reducer Stack**: `TurboshaftBuiltinsAssembler` (combining `TSABuiltinsAssembler` with `BuiltinsReducer`, `FeedbackCollectorReducer`, `MachineLoweringReducer`, and `VariableReducer`) builds a `turboshaft::Graph` directly without `RawMachineAssembler` or `CodeAssembler`.
- **Compilation Job (`TurboshaftAssemblerBuiltinCompilationJob`)**: Defined in `src/compiler/turboshaft/builtin-compiler.{h,cc}` (`TurboshaftPipelineKind::kTSABuiltin`, entered via `BuildWithTurboshaftAssemblerJS`, `BuildWithTurboshaftAssemblerCS`, or `Pipeline::NewBytecodeHandlerCompilationJobTSA`). It runs the TSA generator lambda to populate the Turboshaft graph directly and then invokes the exact same `BuiltinPipeline::OptimizeBuiltin()` and Turboshaft backend phases as CSA.
- **Type-Safe Turboshaft Primitives**:
  - `V<T>`, `ConstOrV<T>`, and `OptionalV<T>` instead of `TNode<T>`.
  - `Label<Ts...>` and `LoopLabel<Ts...>` carrying typed block parameters directly in the type signature (`GOTO(label, args...)`, `GOTO_IF(cond, label, args...)`, ` BIND(label, var...)`), avoiding manual `TVARIABLE` merge lists.
- **Builtin Definitions**: Declared in `src/builtins/builtins-definitions.h` using `TFJ_TSA`, `TFC_TSA`, `TFH_TSA`, and `BCH_TSA` (and experimental Torque-to-TSA generation via `src/torque/tsa-generator.h` when `V8_ENABLE_EXPERIMENTAL_TQ_TO_TSA` is enabled).

---

## 3. Core `CodeStubAssembler` Abstractions

### `TNode<T>` (Typed SSA Values)
Every SSA value in CSA is wrapped in `TNode<T>` (`src/codegen/tnode.h`), where `T` is a compile-time C++ tag type representing either:
- **Tagged V8 Heap Types** (subtypes of `Object`): `Smi`, `HeapObject`, `JSReceiver`, `JSArray`, `String`, `FixedArray`, `Context`, `Map`, `Numeric`, or union types `Union<Smi, HeapNumber>`.
- **Untagged Machine Types** (subtypes of `UntaggedT`): `Int32T`, `Uint32T`, `IntPtrT`, `UintPtrT`, `Word32T`, `WordT`, `Float64T`, `RawPtrT`, `BoolT`.

`TNode<S>` implicitly converts to `TNode<T>` if and only if `is_subtype<S, T>::value` holds at compile time.

### `TVariable<T>` (`TVARIABLE`) and `Label`
Because CSA builds an SSA graph on the fly without an automatic AST-to-SSA dominance frontier pass, mutable variables across basic blocks are modeled explicitly using `TVARIABLE(Type, var_name, initial_value)` (`CodeAssemblerParameterizedLabel` / `CodeAssemblerVariable`) and `Label` (`CodeAssemblerLabel`):
```cpp
TVARIABLE(IntPtrT, index, IntPtrConstant(0));
Label loop(this, &index), done(this);

Goto(&loop);
BIND(&loop);
{
  GotoIf(IntPtrGreaterThanOrEqual(index.value(), length), &done);
  // ... body ...
  index = IntPtrAdd(index.value(), IntPtrConstant(1));
  Goto(&loop);
}
BIND(&done);
```

---

## 4. CSA Sharp Edges and Pitfalls

### 1. Passing Modified `TVARIABLE`s in Loop `Label` Lists (SSA Phi Creation)
When `BIND(&loop)` is executed on a loop header `Label`, CSA has not yet seen the backedge `Goto(&loop)` at the end of the loop body. Therefore, CSA cannot automatically know which `TVariable`s will be mutated inside the loop body.
- **Rule**: Every `TVariable` that is assigned inside a loop (or inside any basic block reachable before jumping back to `loop`) **must** be explicitly listed in the `Label` constructor:
  ```cpp
  Label loop(this, {&var1, &var2});
  ```
- **Why**: Listing `{&var1, &var2}` tells `CodeAssembler` to create incomplete SSA `Phi` nodes at `BIND(&loop)` *before* emitting the loop body, and to wire the backedge values into those `Phi` nodes when `Goto(&loop)` is reached.
- **Failure mode if omitted**: Any instructions emitted inside the loop body before the assignment will silently read the stale pre-loop `TNode` instead of the loop-carried `Phi`, and `Goto(&loop)` will fail with a `DCHECK` crash in `MergeVariables` when it detects that a variable changed along the backedge without a pre-allocated `Phi`.
- *(Note: Non-loop forward-only merge labels `Label done(this)` do not require explicit variable lists unless `Label::kDeferred` is combined with manual merges, because all predecessor `Goto(&done)` jumps are already emitted before `BIND(&done)`.)*

### 2. The CSA Casting Hierarchy: `CAST(x)` vs `UncheckedCast<T>(x)` vs `TrustedCast<T>(x)`
CSA provides three distinct cast operators in `src/compiler/code-assembler.h` (`CheckedNode`), each with strict static and sandbox invariants:

| Cast Macro / Helper | Static Constraint (`static_assert`) | Runtime Behavior in Debug Builds | When to Use |
|---|---|---|---|
| `CAST(x)` (`Cast<T>(x)`) | Requires `types_have_common_values<T, A>`. **Forbids** casting to `TrustedObject` (`!is_subtype_v<A, TrustedObject>`). | When `--slow-debug-code` is enabled, emits a runtime call to `ExternalReference::check_object_type()` that verifies `x` is actually an instance of `T` on the heap. | Default downcast for in-sandbox tagged heap types (`Object` -> `JSArray`, `HeapObject` -> `String`). |
| `UncheckedCast<T>(x)` | Allows casting between compatible representations without subtype overlap. | **Zero runtime check** in all build modes. | When `x`'s type was just checked on the immediately dominating branch, or when reinterpreting untagged/raw representations (`WordT` -> `IntPtrT`). |
| `TrustedCast<T>(x, "legitimation")` | Target `T` **must** be a subtype of `Union<Smi, TrustedObject>`. | When `--slow-debug-code` is enabled, emits `check_object_type()`. | **Mandatory** when casting an untyped `Object`/`HeapObject` to a `TrustedObject` (such as `BytecodeArray`, `Code`, `RegExpData`, `WasmTrustedInstanceData`). |

#### Why `CAST(x)` Forbids `TrustedObject` (`TrustedCast`)
Under the V8 Sandbox threat model, an attacker can corrupt any tagged pointer inside the main pointer-compression cage (`V8HeapCompressionScheme`). `TrustedObject`s live outside the main cage in `TrustedSpace` and must normally be loaded via a `TrustedPointerTable` handle (`LoadTrustedPointerField`) or a `ProtectedPointer` inside another `TrustedObject`. Attempting `CAST(x)` where the target type is a `TrustedObject` fails to compile (`"Casting to TrustedObject is disallowed"`). You must either load the object through a trusted pointer accessor or, if the source is genuinely guaranteed to come from a trusted location (such as a trusted frame slot or trusted FixedArray), use `TrustedCast<MyTrustedType>(x, "reason why source is trusted")`.

### 3. C++ Control Flow (`if` / `for`) vs CSA Graph Control Flow (`Branch` / `Goto`)
- A C++ `if (cond)` executes **once at `mksnapshot` compile time** to decide which graph nodes to emit.
- A runtime branch in generated machine code must use `Branch(tnode_cond, &if_true, &if_false)` or `GotoIf` / `GotoIfNot`.
- Every `Label` that is jumped to must eventually be bound with `BIND(&label)`, and every basic block must end with a terminator (`Goto`, `Branch`, `Return`, `TailCallBuiltin`, `TailCallRuntime`, or `Unreachable`) before binding the next label.

---

## 5. Key Files Reference

| File | Purpose |
|---|---|
| `src/compiler/code-assembler.{h,cc}` | `CodeAssembler` base class, `TVariable`, `CodeAssemblerLabel`, `CodeAssemblerParameterizedLabel`, `CAST`/`UncheckedCast`/`TrustedCast`, and `BuiltinCompilationScheduler`. |
| `src/codegen/code-stub-assembler.{h,cc}` | `CodeStubAssembler` with V8 heap object accessors, allocations, Smi/Tagged operations, string/array helpers, and IC primitives. |
| `src/codegen/tnode.h` | `TNode<T>`, `SloppyTNode<T>`, tag type hierarchy (`Object`, `HeapObject`, `TrustedObject`, `UntaggedT`), and `is_subtype` traits. |
| `src/codegen/turboshaft-builtins-assembler-inl.h` | `TurboshaftBuiltinsAssembler` (TSA), `BuiltinsReducer`, and direct Turboshaft builtin generation macros. |
| `src/compiler/turboshaft/builtin-compiler.{h,cc}` | `BuildWithTurboshaftAssemblerJS`, `BuildWithTurboshaftAssemblerCS`, and `TurboshaftAssemblerBuiltinCompilationJob`. |
| `src/compiler/turboshaft/pipelines.{h,cc}` | `BuiltinPipeline::OptimizeBuiltin()` orchestrating the Turboshaft CSA/TSA optimization phases. |
| `src/compiler/turboshaft/csa-optimize-phase.{h,cc}` | Implementation of `CsaEarlyMachineOptimizationPhase`, `CsaLoadEliminationPhase`, `CsaLateEscapeAnalysisPhase`, `CsaBranchEliminationPhase`, and `CsaOptimizePhase`. |
