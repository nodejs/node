# V8 Torque Architecture

Torque (`src/torque/`) is V8's domain-specific language and ahead-of-time compiler for authoring builtins, runtime helpers, and V8 heap object layout definitions (`.tq` files). During the V8 build (prior to `mksnapshot`), the `torque` executable compiles all `.tq` files into:
1. C++ `CodeStubAssembler` (CSA) subclasses (`*FromDSLAssembler`, or experimental `TurboshaftBuiltinsAssembler` TSA code under `V8_ENABLE_EXPERIMENTAL_TQ_TO_TSA`).
2. C++ heap object layout headers, field offset constants, accessors, body descriptors, heap verifiers, and post-mortem debug readers in `out/<build>/gen/torque-generated/`.

---

## 1. The Torque Compilation Pipeline

`CompileTorque` and `CompileCurrentAst` (`src/torque/torque-compiler.cc`) execute a multi-stage pipeline over all `.tq` source files listed in `BUILD.gn`:

```
.tq Source Files  +  tools/metagen C++ Layout JSON (optional)
   │
   ▼
1. TorqueParser::ParseTorque        (src/torque/torque-parser.{h,cc})
   │  Earley-style grammar parser that builds the AST (CurrentAst) in GlobalContext.
   ▼
2. ImportCppLayouts (optional)      (src/torque/layout-loader.{h,cc})
   │  Imports C++ V8_OBJECT class layouts extracted by tools/metagen.
   ▼
3. PredeclarationVisitor            (src/torque/predeclaration-visitor.{h,cc})
   │  Two-step Predeclare() + ResolvePredeclarations() registering all namespaces,
   │  type aliases, classes, structs, and generic callables regardless of order.
   ▼
4. DeclarationVisitor               (src/torque/declaration-visitor.{h,cc})
   │  Resolves macro, builtin, runtime, intrinsic, and constant signatures.
   ▼
5. TypeOracle::FinalizeAggregateTypes()  (src/torque/type-oracle.{h,cc})
   │  Resolves all StructType and ClassType fields, computes field byte offsets
   │  and object sizes, and allows mutually recursive class field references.
   ▼
6. VerifyCppLayouts                 (src/torque/layout-loader.{h,cc})
   │  Cross-checks finalized Torque class/struct/bitfield layouts against C++
   │  V8_OBJECT definitions in the metagen layout JSON.
   ▼
7. ImplementationVisitor            (src/torque/implementation-visitor.{h,cc})
      Lowers callables into a stack-typed ControlFlowGraph (CFG), inlines macros,
      runs CFG optimization, and emits C++ CSA/header files into torque-generated/.
```

---

## 2. Aggregate Layout Finalization (`FinalizeAggregateTypes`) and `VerifyCppLayouts`

### `TypeOracle::FinalizeAggregateTypes()` (`src/torque/type-oracle.h`)
During `PredeclarationVisitor`, `ClassType` and `StructType` instances are created as shells so two classes can reference each other in their field declarations without circular dependency errors. Once all declarations are registered, `TypeOracle::FinalizeAggregateTypes()` iterates through `aggregate_types_` and calls `Finalize()` on each `AggregateType`:
- Resolves every field's `Type*` and validates field alignment and tagging constraints (`Tagged<T>`, `Weak<T>`, `TrustedPointer<T>`, `ProtectedPointer<T>`, `ExternalPointer<T>`, unaligned or scalar fields).
- Computes exact byte offsets relative to the superclass header size, tracks indexed slice/array tail fields (`V8_TQ_TAIL_LENGTH`), and determines the total instance size.

### Cross-Checking C++ `V8_OBJECT` Layouts (`VerifyCppLayouts`)
As V8 migrates object definitions from `.tq` files to C++ `V8_OBJECT` classes (`include/v8-object.h` and `src/objects/object-macros.h`), `tools/metagen` extracts C++ class, field, and bitfield metadata into a layout JSON file. In `CompileCurrentAst` (`src/torque/torque-compiler.cc`):
- `ImportCppLayouts` (`src/torque/layout-loader.cc`) can import C++ class and `bitfield struct` layouts directly from the metagen JSON.
- Immediately after `TypeOracle::FinalizeAggregateTypes()`, `VerifyCppLayouts(options.layout_json_path)` runs `LayoutVerifier::Run`:
  - Matches every Torque class/bitfield struct against its C++ `V8_OBJECT` counterpart (`records_by_torque_name_` / `records_by_cpp_name_`).
  - Verifies inheritance hierarchy, field names, field order, field types (`V8_TQ_TYPE`), constness (`V8_TQ_CONST`), array tail lengths, and bitfield widths/positions, reporting exact source-position diagnostics if a C++ `V8_OBJECT` class and `.tq` declaration diverge.

---

## 3. Stack-Typed CFG, `StackRange`s, and `LocationReference` Lowering

Unlike JavaScript or C++, Torque's intermediate representation (`ControlFlowGraph`, `Block`, and `CfgAssembler` in `src/torque/cfg.{h,cc}`) operates on an **abstract typed value stack** (`Stack<const Type*>`).

### Lowering Compound Types into Multi-Slot `StackRange`s
In Torque, only single-word machine/tagged types (`Object`, `Smi`, `intptr`, `uint32`, `RawPtr`, etc.) map to a single CSA `TNode<T>` stack slot. Compound value types do **not** exist at runtime as heap objects; instead, `LowerType(const Type*)` (`src/torque/types.cc`) flattens them recursively into a sequence of scalar constituent types occupying a contiguous `StackRange` (`BottomOffset` `[ begin, end )`) on the `CfgAssembler` stack:
- **`struct` (`StructType`)**: Lowered to the concatenated `LowerType(field)` list of all its fields in declaration order. A `VisitResult` for a struct holds its `Type*` and a `StackRange` spanning N stack slots (or a compile-time `constexpr` string expression).
- **`Reference<T>` / `ConstReference<T>`**: Lowered into a 2-slot `StackRange`: `(HeapObject, intptr)` representing `(base_object, untagged_byte_offset)`.
- **`Slice<T>` (`TorqueStructSlice`)**: Lowered into a 3-slot `StackRange`: `(Object, intptr, intptr)` representing `(base_object, start_byte_offset, length)`.

When control flow merges at a `Block`, `Block::MergeInputDefinitions` (`src/torque/cfg.h`) compares the `Stack<DefinitionLocation>` from each incoming predecessor slot-by-slot and creates a `DefinitionLocation::Phi(block, offset)` for any stack slot whose definition differs across predecessors. When `ImplementationVisitor` emits CSA code for the CFG, only those stack slots marked as `Phi` allocate a `CodeAssemblerParameterizedLabel` / `Phi` node in CSA.

### `LocationReference` Variants (`src/torque/implementation-visitor.h`)
L-values and assignable storage locations in Torque are represented by `LocationReference`, which encapsulates five distinct access modes without materializing intermediate pointers:

| `LocationReference` Kind | Factory Method | Underlying Storage | How Reads (`Fetch`) and Writes (`Assign`) Lower |
|---|---|---|---|
| **`VariableAccess`** | `LocationReference::VariableAccess(VisitResult, binding)` | A mutable local variable's `StackRange` on the current `CfgAssembler` stack. | Reads copy the `StackRange`; writes `Poke` new values directly into the variable's `StackRange` slots (updating SSA definitions across blocks). |
| **`Temporary`** | `LocationReference::Temporary(VisitResult, description)` | A read-only r-value or macro parameter `VisitResult`. | Allows reading (`temporary()`), including reading individual struct fields by slicing the `StackRange`, but forbids assignment (preventing mutation of temporary r-value structs). |
| **`HeapReference`** | `LocationReference::HeapReference(VisitResult)` | A 2-slot `Reference<T>` `(HeapObject, intptr offset)` on the stack. | Dispatches to `LoadReference<T>` (`LoadObjectField` / `LoadTrustedPointerField`) on read and `StoreReference<T>` (with GC write barrier or `StoreObjectFieldNoWriteBarrier`) on write. |
| **`HeapSlice`** | `LocationReference::HeapSlice(VisitResult)` | A 3-slot `Slice<T>` `(Object, intptr offset, intptr length)` over an indexed class field. | Indexing `slice[i]` bounds-checks `i < length` (branching to an out-of-bounds label or `Unreachable`) and returns a `HeapReference` at `offset + i * sizeof(T)`. |
| **`BitFieldAccess`** | `LocationReference::BitFieldAccess(LocationReference base, BitField)` | A nested `LocationReference` to the integer container word plus a `BitField` struct descriptor. | Reads fetch the container word and emit `DecodeWord32`/`DecodeWord` (shift + mask); writes fetch the container, clear and insert the shifted bitfield value, and write the updated word back to `base`. |

---

## 4. Zero-Cost Macro Inlining and `label` Wiring (`InlineMacro`)

Torque `macro`s are **always inlined** into their caller during AST-to-CFG lowering (`ImplementationVisitor::InlineMacro` in `src/torque/implementation-visitor.cc`). There is no runtime call overhead for a Torque `macro` (unlike a `builtin` or `runtime` function).

### How `ImplementationVisitor::InlineMacro` Works
When `ImplementationVisitor` visits a call to `Macro* macro` with optional `this_reference`, evaluated `arguments`, and caller `label_blocks`:
1. **Recursion Check (`MacroInliningScope`)**: Inserts `macro` into `inlining_macros_`. Because macros are unconditionally inlined at compile time, recursive `macro` calls are rejected with a compile error (recursive helpers must be declared as `builtin`).
2. **Parameter Binding (`parameter_bindings`)**:
   - For methods (`this_reference.has_value()`), binds `this` (`kThisParameterName`) directly to `LocalValue{*this_reference}`. If the caller invoked the method on a mutable variable (`VariableAccess` or `HeapReference`), the method can mutate `this`'s fields in place; if invoked on a temporary r-value (`Temporary`), attempts to mutate `this` fail at compile time.
   - For each value parameter, binds the parameter name to `LocationReference::Temporary(arg, "parameter " + name)` in `ValueBindingsManager`.
3. **Direct `label` Wiring (`label_bindings`)**:
   - In Torque, a macro signature can declare exceptional or multi-exit continuations:
     ```torque
     macro TryFastPath(x: Object): Smi labels SlowPath, Bailout(Object)
     ```
   - At the call site (`TryFastPath(val) otherwise goto HandleSlow, HandleBailout(err)`), the caller creates or looks up a CFG `Block*` for each target label (`label_blocks`).
   - `InlineMacro` binds each formal label name in `macro->signature().labels` directly to `LocalLabel{label_blocks[i], label_info.types}` in `LabelBindingsManager`.
   - When the macro body executes `goto SlowPath` or `goto Bailout(obj)`, `GenerateBranch` pushes any label payload arguments onto the stack and emits a `GotoInstruction` jumping **directly to the caller's `Block*`**, with zero boolean status flags or tuple unpacking.
4. **Return Value Merging (`_macro_end`)**:
   - If `macro` can return (`return_type != Never`), `InlineMacro` allocates a `macro_end` `Block` whose input stack template consists of the caller's stack followed by `LowerType(return_type)` slots, and binds it to the internal label `_macro_end` (`kMacroEndLabelName`).
   - Every `return expr` statement inside the macro body lowers `expr` onto the stack and jumps to `_macro_end`.
   - After visiting `macro->body()`, if `_macro_end` is reachable (`HasInputDefinitions()`), `InlineMacro` binds `macro_end` as the current block and returns the `VisitResult` referencing the top `LowerType(return_type).size()` slots of the stack.

---

## 5. Map of Generated Files in `out/<build>/gen/torque-generated/`

Running the `torque` compiler generates the following files under `out/<build>/gen/torque-generated/`, which are included across `src/`:

| Generated File Path (under `out/<build>/gen/torque-generated/`) | Emitted By (`ImplementationVisitor`) | Contents and Consumers |
|---|---|---|
| `src/.../*-tq-csa.{h,cc}` | `BeginGeneratedFiles` / `EndGeneratedFiles` | Per-`.tq`-file `*FromDSLAssembler` classes containing the generated C++ `CodeStubAssembler` code for all Torque macros and builtins defined in that `.tq` file. |
| `src/.../*-tq-inl.inc` and `*-tq.inc` / `*-tq.cc` | `GenerateClassDefinitions` | Generated inline field accessors, constructors, `Cast` helpers, and layout verification asserts included by `src/objects/*-inl.h` and `src/objects/*.h`. |
| `instance-types.h` | `GenerateInstanceTypes` | The `INSTANCE_TYPE_LIST` and `InstanceType` enum values computed from the Torque class hierarchy (assigning contiguous ranges for `is_subtype` range checks). |
| `builtin-definitions.h` | `GenerateBuiltinDefinitionsAndInterfaceDescriptors` | `BUILTIN_LIST_FROM_TORQUE` macro listing all `TFJ`, `TFS`, and `TFC` builtins authored in `.tq` files, included by `src/builtins/builtins-definitions.h`. |
| `interface-descriptors.inc` | `GenerateBuiltinDefinitionsAndInterfaceDescriptors` | Auto-generated `CallInterfaceDescriptor` definitions for Torque builtins with custom stub signatures. |
| `bit-fields.h` | `GenerateBitFields` | `base::BitField<T, shift, size>` C++ definitions generated from `bitfield struct` declarations in `.tq` files. |
| `exported-macros-assembler.{h,cc}` | `GenerateExportedMacrosAssembler` | `ExportedMacrosAssembler` class exposing `@export` Torque macros so hand-written C++ `CodeStubAssembler` builtins can call them directly. |
| `csa-types.h` | `GenerateCSATypes` | `TorqueStruct*` C++ struct wrappers (lists of `TNode<T>` fields) corresponding to multi-slot Torque `struct` types. |
| `field-offsets.h` | `GenerateClassDefinitions` | `TORQUE_GENERATED_*_FIELDS` offset macros for heap objects still defined in `.tq`. |
| `class-verifiers.{h,cc}` | `GenerateClassDefinitions` | `TorqueGeneratedClassVerifiers` implementing `HeapVerifier` field-type checks (`--verify-heap`) for every Torque-defined class. |
| `enum-verifiers.cc` | `GenerateEnumVerifiers` | C++ `static_assert` checks validating that `extern enum` declarations in `.tq` match the C++ enum constants. |
| `objects-printer.cc` & `objects-body-descriptors-inl.inc` | `GenerateClassDefinitions` | `Object::Print` debug printers and GC `BodyDescriptor` definitions for Torque heap classes. |
| `class-debug-readers.{h,cc}` | `GenerateClassDebugReaders` | Post-mortem debugging readers used by `tools/debug_helper` (LLDB / WinDbg / crash-dump inspection). |

---

## 6. Key Source Files Reference

| File | Purpose |
|---|---|
| `src/torque/torque.cc` | CLI entry point for the `torque` compiler executable. |
| `src/torque/torque-compiler.{h,cc}` | `CompileTorque` and `CompileCurrentAst` orchestrating parsing, layout verification, and code generation. |
| `src/torque/torque-parser.{h,cc}` | Grammar rules and AST construction for `.tq` files. |
| `src/torque/ast.h` | AST node definitions (`Expression`, `Statement`, `Declaration`, `TypeExpression`). |
| `src/torque/types.{h,cc}` | `Type`, `AggregateType`, `StructType`, `ClassType`, `BitFieldStructType`, and `LowerType`. |
| `src/torque/type-oracle.{h,cc}` | Singleton registry of all Torque types and `TypeOracle::FinalizeAggregateTypes()`. |
| `src/torque/layout-loader.{h,cc}` | `VerifyCppLayouts` (`LayoutVerifier`) and `ImportCppLayouts` (`LayoutImporter`) cross-checking `.tq` layouts with `tools/metagen` C++ `V8_OBJECT` JSON. |
| `src/torque/cfg.{h,cc}` & `instructions.{h,cc}` | `ControlFlowGraph`, `Block`, `CfgAssembler`, `Stack<T>`, `StackRange`, and Torque IR instructions. |
| `src/torque/implementation-visitor.{h,cc}` | `ImplementationVisitor`, `LocationReference`, `VisitResult`, `InlineMacro`, and C++/CSA file emitters. |
| `src/torque/csa-generator.{h,cc}` | Translates Torque CFG instructions into C++ `CodeAssembler` / `CodeStubAssembler` calls. |
| `src/torque/tsa-generator.{h,cc}` | Experimental generator translating Torque AST/CFG directly to `TurboshaftBuiltinsAssembler` (TSA). |
