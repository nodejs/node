# Holes and Internal Sentinels in V8

## Conceptual Overview

In JavaScript, an array can have missing elements (such as `const a = [1, , 3]`), a `let` or `const` variable cannot be read before its lexical declaration is evaluated (the Temporal Dead Zone, or TDZ), and internal hash tables or property cells need a marker for deleted or uninitialized slots. Representing these states with `undefined` would be ambiguous because a user program can explicitly store `undefined` (`a[1] = undefined` vs `delete a[1]`).

V8 solves this by using internal **sentinel objects** called **Holes**, along with a special floating-point bit pattern (**Hole NaN**) for unboxed double arrays. Because a sentinel object must never leak to user JavaScript (where it would violate ECMAScript invariants or break type assumptions in optimizing compilers), V8 enforces strict architectural boundaries around how holes are represented, segregated by subsystem, initialized during two-phase object creation, and guarded in memory.

---

## 1. Segregated Hole Roots (`Hole` Hierarchy)

Historically, V8 reused a single `the_hole_value` across arrays, hash tables, property cells, closures, and lexical TDZ variables. Sharing one sentinel across unrelated subsystems created subtle security and correctness hazards: if a bug allowed a hash-table deletion marker or an uninitialized property cell to flow into code that checked for `the_hole`, one subsystem's sentinel could be misinterpreted by another.

Modern V8 segregates internal sentinels into distinct `Hole` subclasses declared in `HOLE_LIST` (`src/objects/object-list-macros.h`, `src/objects/hole.h`) and pre-allocated as immutable roots in `ReadOnlyRoots` (`src/roots/roots.h`). Every `Hole` object shares the single `hole_map` (`InstanceType::HOLE_TYPE`, inheriting from `HeapObjectLayout`), so they are indistinguishable by `Map` or `InstanceType` and must be compared by direct root pointer identity (`Tagged<Object> == ReadOnlyRoots(isolate).<name>()` or `RootIndex::k<Name>`).

| Root Accessor (`ReadOnlyRoots`) | C++ Type (`src/objects/hole.h`) | Subsystem & Exact Invariant |
| :--- | :--- | :--- |
| `the_hole_value()` | `TheHole` | **Array and Dictionary Elements ONLY.** Marks missing elements in `FixedArray` (`HOLEY_SMI_ELEMENTS`, `HOLEY_ELEMENTS`) and `NumberDictionary` (`DICTIONARY_ELEMENTS`). Must never be used for new non-element purposes. |
| `tdz_hole_value()` | `TdzHole` | **Lexical Temporal Dead Zone (TDZ) & Uninitialized `this`.** Marks uninitialized `let`, `const`, `class`, and `using` bindings in stack registers (`LdaTdzHole`), `Context` slots, `ContextCell`s, and `ScriptContextTable` slots, as well as the uninitialized `this` receiver in derived class constructors before `super()` and placeholder receivers in `ConstructAsBuiltin` / `JSBuiltinsConstructStub` / API constructors. (When `V8_ENABLE_TDZ_HOLE` is disabled, `TdzHole` aliases `TheHole` while `disabled_tdz_hole_value()` occupies the root slot.) |
| `property_cell_hole_value()` | `PropertyCellHole` | **Global `PropertyCell`s.** Marks deleted or uninitialized properties in a `GlobalDictionary` `PropertyCell`. |
| `hash_table_hole_value()` | `HashTableHole` | **Hash Tables.** Marks deleted (tombstone) entries in `HashTable`-derived containers (such as `NameDictionary`, `OrderedHashMap`, `OrderedHashSet`, `WeakArrayList`). |
| `promise_hole_value()` | `PromiseHole` | **Promises.** Internal sentinel used in promise resolution and reaction chains. |
| `uninitialized_value()` | `UninitializedHole` | **Two-Phase Allocation & IC Sentinels.** Marks fields or slots during two-phase fast literal boilerplate allocation before dynamic property values are written, and serves as the initial state of uninitialized IC slots. |
| `arguments_marker()` | `ArgumentsMarker` | **Unmaterialized `arguments` Object.** Used in optimized frames and deoptimizer frame translations when an `arguments` object has been escape-analyzed away. |
| `optimized_out()` | `OptimizedOut` | **Dead/Elided Values in Deoptimization.** Placed into reconstructed stack slots (`TranslationOpcode::OPTIMIZED_OUT`) when a variable's live range has ended and the register/stack value was not preserved across a safepoint. |
| `stale_register()` | `StaleRegister` | **Clobbered Bytecode Registers.** Marks bytecode registers whose values are no longer valid. |
| `exception()` | `ExceptionHole` | **C++ Runtime Exception Return.** Returned by C++ `RUNTIME_FUNCTION` implementations (and `Failure` paths) to indicate that an exception is pending on `Isolate::pending_exception()`. |
| `termination_exception()` | `TerminationException` | **Uncatchable Execution Termination.** Stored in `Isolate::pending_exception()` when execution is forcibly aborted via `Isolate::TerminateExecution()`. Bypasses user `try`/`catch` and `finally` blocks. |
| `self_reference_marker()` | `SelfReferenceMarker` | **Recursive Self-Reference.** Used during compilation/serialization to represent recursive self-references. |
| `basic_block_counters_marker()` | `BasicBlockCountersMarker` | **PGO Profiling.** Marker used by basic-block profiling instrumentation (`--turbo-profiling`). |

### Preventing Accidental `the_hole` and `tdz_hole` Leaks

Because `the_hole` is restricted to array and dictionary element backing stores, V8 provides `DisallowTheHole` checks and dedicated accessor wrappers in object and runtime code. Subsystem-specific accessors (such as `PropertyCell::value()`, `Context::get()`, or `HashTable::KeyAt()`) check for their respective segregated hole (`property_cell_hole`, `tdz_hole`, `hash_table_hole`). If `the_hole` ever escapes into a general expression value, debug checks (`DCHECK(!IsTheHole(value))`) fail immediately.

Similarly, because `tdz_hole` (`TdzHole`) is used as the placeholder frame receiver in derived class constructors before `super()` and in builtin/API constructor frames (`ConstructAsBuiltin` / `JSBuiltinsConstructStub` / `HandleApiCallHelper`), all three stack-trace receiver consumers must check `IsTdzHole` (rather than `IsTheHole`) before inspecting the frame receiver:
- **`CallSiteBuilder::AppendFrame` (`src/execution/isolate.cc`)**: Replaces `IsTdzHole(*receiver_or_instance)` with `undefined_value()` when capturing `Error.stack` frames.
- **`StringStream::PrintPrototype` (`src/strings/string-stream.cc`)**: Checks `IsTdzHole(receiver)` (alongside `IsNullOrUndefined(receiver)`) during `Isolate::PrintStack` / `console.trace()` so it does not call `Object::GetPrototypeChainRootMap` on `TdzHole`.
- **`DebugStackTraceIterator::GetReceiver` (`src/debug/debug-stack-trace-iterator.cc`)**: Checks `IsTdzHole(*value)` (for both context `this` slots and `frame_inspector_->GetReceiver()`) and returns an empty `v8::MaybeLocal<v8::Value>()` instead of exposing `TdzHole` to the debugger.

---

## 2. Unmapped Hole Guard Pages (`--unmap-holes`)

In V8's Sandbox threat model, if a compiler bug allows an internal `Hole` object to escape to user JavaScript without a hole check, subsequent code might treat the `Hole` as a valid `JSReceiver` or `HeapObject` and dereference fields at arbitrary in-object offsets (for example, reading `obj->properties_or_hash()` or an in-object property slot).

To turn accidental hole dereferences into immediate, unexploitable hardware faults, every `Hole` instance in `ReadOnlySpace` includes a 64 KB payload (`src/objects/hole.h`):

```cpp
V8_OBJECT class Hole : public HeapObjectLayout {
 public:
#ifdef V8_ENABLE_SANDBOX
  static constexpr int kPayloadSize = 64 * KB;
  char payload_[kPayloadSize];
#endif
};
```

When `--unmap-holes` is active (on supported OS/page-size configurations under `V8_ENABLE_SANDBOX`):
1. Each `Hole` object is allocated such that its `payload_` spans OS-aligned pages in `ReadOnlySpace`.
2. During initialization (`Hole::Initialize`), V8 fills `payload_` with `kHoleNanUpper32` (`0xFFF7FFFF`) and unmaps the OS pages covering `payload_` via `OS::DecommitPages` / page permissions.
3. The first word of the `Hole` object (its `map_word` pointing to `hole_map`) remains readable so `IsHole()` / `InstanceType` checks still work when needed, while any attempt to read or write fields inside the object body faults immediately in hardware (`SIGSEGV` / `SIGBUS`).

---

## 3. Floating-Point Hole NaN (`kHoleNanInt64`)

Unboxed double arrays (`FixedDoubleArray` backing `PACKED_DOUBLE_ELEMENTS` and `HOLEY_DOUBLE_ELEMENTS`) store raw 64-bit IEEE-754 floating-point numbers (`double`) rather than tagged pointers. They cannot store a tagged pointer to `TheHole`.

Instead, V8 reserves a specific quiet/signaling NaN bit pattern (`src/common/globals.h`, `src/objects/fixed-primitive-array.h`) to represent an element hole in `FixedDoubleArray` (and inside `UninitializedHeapNumber` boxes):

```cpp
// Standard IEEE-754 platforms (x64, ia32, arm, arm64, etc.):
constexpr uint32_t kHoleNanUpper32 = 0xFFF7FFFF;
constexpr uint32_t kHoleNanLower32 = 0xFFF7FFFF;
constexpr uint64_t kHoleNanInt64 =
    (static_cast<uint64_t>(kHoleNanUpper32) << 32) | kHoleNanLower32;
// kHoleNanInt64 == 0xfff7fffffff7ffff
```

*(Note: When `V8_ENABLE_UNDEFINED_DOUBLE` is enabled, V8 also defines `kUndefinedNanInt64 = 0xfff6fffffff6ffff` to represent `undefined` directly inside double arrays/fields without transitioning to tagged elements.)*

### NaN Canonicalization vs Hole Spoofing

A subtle hazard arises because JavaScript programs can construct arbitrary 64-bit float bit patterns using typed arrays (`new Float64Array(new BigUint64Array([0xfff7fffffff7ffffn]).buffer)[0]`) or `DataView.prototype.getFloat64()`. If V8 stored raw floating-point values directly into a `HOLEY_DOUBLE_ELEMENTS` array or constant pool without distinguishing user NaNs from hole NaNs, user code could forge an array hole!

V8 prevents hole spoofing across all tiers through strict canonicalization and dual literal tags:

1. **Runtime & IC Stores (`FixedDoubleArray::set`)**:
   - `FixedDoubleArray::set(int index, double value)` canonicalizes any input NaN (`std::isnan(value)`) to a safe quiet NaN (`std::numeric_limits<double>::quiet_NaN()`, `0x7ff8000000000000`) before writing to memory, and verifies `DCHECK(!is_the_hole(index))`.
   - Only `FixedDoubleArray::set_the_hole(index)` (or `FixedDoubleArray::set_raw(index, Float64)` used when copying raw elements from an already-validated source) can write `kHoleNanInt64`.
2. **Compiler & Deoptimizer Literal Representation (`DOUBLE_LITERAL` vs `HOLEY_DOUBLE_LITERAL`)**:
   - In `Float64` (`src/base/numbers/double.h`), `Float64::FromBits(kHoleNanInt64)` preserves the exact 64-bit pattern (`Float64::is_hole_nan()`).
   - When TurboFan (`CodeGenerator::AddTranslationForOperand` in `src/compiler/backend/code-generator.cc`), Turboshaft (`TurbolevGraphBuilder::AddVirtualObjectNestedValue` in `src/compiler/turboshaft/turbolev-graph-builder.cc`), or Maglev (`MaglevFrameTranslationBuilder` in `src/maglev/maglev-code-generator.cc`) encodes float64 constants into deoptimization frame translations (`src/deoptimizer/frame-translation-builder.cc`, `src/deoptimizer/translation-opcode.h`), it uses two distinct 64-bit translation opcodes rather than laundering hole NaNs through `LITERAL(the_hole_value)`:
     - `TranslationOpcode::DOUBLE_LITERAL` (`StoreDoubleLiteral(Float64)` -> `TranslatedValue::NewDouble`): Used for `Float64Constant` (`MachineType::Float64()`). Even if a user program produced the `kHoleNanInt64` bit pattern in a float64 constant placed into a tagged `FixedArray` slot (`InitializeObjectWithTaggedFieldsAt`), `TranslatedValue::GetRawValue` and `GetValue` treat `kDouble` as a normal number and canonicalize `double_value().is_nan()` to `std::numeric_limits<double>::quiet_NaN()` when allocating a `HeapNumber`, never materializing `the_hole_value` or `undefined_value`.
     - `TranslationOpcode::HOLEY_DOUBLE_LITERAL` (`StoreHoleyDoubleLiteral(Float64)` -> `TranslatedValue::NewHoleyDouble`): Used for `HoleyFloat64Constant` (`MachineType::HoleyFloat64()`). When materialized as a tagged value via `TranslatedValue::GetRawValue()`, `is_hole_nan()` (and `is_undefined_nan()`) returns `ReadOnlyRoots(isolate()).undefined_value()`. When materializing a `FixedDoubleArray` (`TranslatedState::MaterializeFixedDoubleArray`), the deoptimizer calls `TranslatedValue::GetDoubleValue()` and writes the raw `Float64` directly via `FixedDoubleArray::set_raw(i, value)` (preserving `kHoleNanInt64` for array holes), whereas `TranslatedState::MaterializeHeapNumber` canonicalizes `value.is_nan()` to `Float64::quiet_nan()`.

---

## 4. Two-Phase Object and Array Literal Initialization

When V8 creates an object or array literal (`{ a: 1, b: foo() }` or `[1, foo(), 3]`), it often clones a pre-built boilerplate `Map` and backing structure, then evaluates dynamic property/element expressions (`foo()`). Evaluating `foo()` (or allocating nested objects or `HeapNumber` boxes) can trigger a **Garbage Collection (GC)** or a **lazy deoptimization** mid-initialization.

This creates a critical GC and deoptimizer invariant:
> **Every allocated heap object and backing store must be in a GC-valid, walkable state before any allocation or dynamic call can occur.** Specifically:
> - A `Representation::Double()` field in a `JSObject`'s `Map` tells the GC / heap verifier / object printer that the field contains a valid pointer to a `HeapNumber` (never `uninitialized_value` or `Smi::zero()`).
> - A `FixedArray` or `FixedDoubleArray` must have all slots initialized before GC scans it.

V8 enforces this using **two-phase initialization** across the runtime, parser/boilerplate builder, and optimizing compilers:

### 1. Runtime & JSON/Literal Builder (`JSDataObjectBuilder` & `Runtime_CreateObjectLiteral`)

When constructing an object with a known target `Map` that contains `Representation::Double()` fields (`src/objects/js-data-object-builder-inl.h`):
- **Pre-allocating `HeapNumber` storage (`FoldedMutableHeapNumberAllocation`)**: Before allocating the `JSObject` itself, `JSDataObjectBuilder::CreateAndInitialiseObject` calculates `extra_heap_numbers_needed_` and pre-allocates a single `ByteArray` large enough to hold all required `HeapNumber` boxes (`extra_heap_numbers_needed_ * sizeof(HeapNumber)`).
- **Atomic Phase 1 + Phase 2 under `DisallowGarbageCollection`**: Once the `ByteArray` and the `JSObject` (`NewJSObjectFromMap`) are allocated, a `FoldedMutableHeapNumberAllocator` carves individual `HeapNumber` boxes out of the `ByteArray` in-place without triggering GC:
  - If the double value is already known, it initializes a `HeapNumber` (`heap_number_map`) with the float64 value.
  - If the double field is still uninitialized (for example, during boilerplate construction before dynamic evaluation), `AllocateUninitialized()` stamps an **`UninitializedHeapNumber`** (`uninitialized_heap_number_map` with value `kHoleNanInt64`) and stores that pointer into the `JSObject` via `RawFastInobjectPropertyAtPut`.
  - After all double fields hold valid `HeapNumber` / `UninitializedHeapNumber` pointers, the `ByteArray`'s length is shrunk to `0` (after `EnsureSweepingCompletedForObject`) so the GC treats the carved-out regions as independent `HeapNumber` objects.
- Similarly, when cloning a boilerplate in `JSObjectWalkVisitor::StructureWalk` (`src/runtime/runtime-literals.cc`), any `Representation::Double()` field whose boilerplate value is `IsUninitializedHeapNumber` has a fresh `NewUninitializedHeapNumber()` allocated so the clone never aliases the boilerplate's mutable `HeapNumber` box.
- For double array literals (`CreateLiteralWithoutAllocationSite`), `Factory::NewFixedDoubleArray` initializes slots with `kHoleNanInt64` before `CopyFrom` or dynamic element stores occur.

### 2. Optimizing Compilers (`JSCreateLowering::TryAllocateFastLiteral` & Turboshaft)

When TurboFan (`JSCreateLowering::TryAllocateFastLiteral` in `src/compiler/js-create-lowering.cc`), Maglev (`BuildAllocateFastObject`), or Turboshaft inlines fast object/array literal creation:
1. **Phase 1 (Safe Skeleton Initialization)**:
   - Immediately after `BeginRegion` / `Allocate`, the compiler emits stores that initialize the `Map`, `properties`, `elements`, and all in-object fields.
   - Fields whose values depend on not-yet-evaluated subexpressions are pre-initialized with safe sentinels: `uninitialized_value` for tagged fields, or a newly allocated `UninitializedHeapNumber` (`kHoleNanInt64`) for `Representation::Double()` fields.
   - In Turboshaft (`RedundantStoreAnalysis::ProcessBlock` in `src/compiler/turboshaft/store-store-elimination-reducer-inl.h`), any operation with `effects.requires_consistent_heap()` transitions tracked unobservable stores to **`StoreObservability::kGCObservable`** (`MarkAllStoresAsGCObservable()`). When walking backward and encountering a `StoreOp` in the `kGCObservable` state, if `store.maybe_initializing_or_transitioning` is true, `RedundantStoreAnalysis` **cannot** eliminate the Phase 1 initializing store (instead calling `table_.MarkStoreAsUnobservable(store.base(), store.offset, size)` so earlier stores to the same field can be eliminated), whereas a non-initializing store (`!store.maybe_initializing_or_transitioning`) is added to `eliminable_stores_`. Across loop headers (`block.IsLoop()`), `table_.InvalidateBasesInRange(block_index, back_edge->index())` resets stores to objects allocated inside the loop back to `kObservable` so stores from one iteration never eliminate initializing stores in an earlier iteration.
2. **Phase 2 (Dynamic Expression Evaluation & Final Stores)**:
   - After `FinishRegion` completes the skeleton object, the compiler evaluates the dynamic subexpressions (which may allocate, call arbitrary JS, or trigger GC/deopt) and overwrites the Phase 1 sentinel slots with the final values.

---

## Related Documentation

- **[Hole Check Elision Overview](../parsing/hole-check-elision.md)**: How `tdz_hole` checks (`ThrowReferenceErrorIfTdzHole`, `ThrowSuperNotCalledIfTdzHole`) are eliminated across the Parser, BytecodeGenerator, and Maglev/TurboFan.
- **[Fields and Elements](fields-and-elements.md)**: How `HOLEY_*` vs `PACKED_*` `ElementsKind` transitions and in-object `Representation::Double()` fields work.
- **[Deoptimization](../runtime/deoptimization.md)**: How `HOLEY_DOUBLE_LITERAL` and `OPTIMIZED_OUT` are materialized by `TranslatedState`.
