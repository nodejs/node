# V8 Deoptimization Architecture

## Conceptual Overview

Optimizing compilers in V8 (Maglev and TurboFan/Turboshaft) achieve high performance by **speculating** based on historical execution feedback collected by Ignition and Sparkplug. For example, if a `+` operator has only ever seen `Smi` (Small Integer) inputs, the compiler emits fast machine instructions for 32-bit integer addition with overflow checking rather than full ECMAScript type-coercion logic.

Whenever an assumption is violated at runtime -- for example, a function suddenly receives a `String` where a `Smi` was expected, an integer addition overflows, or a global prototype/field constancy assumption (`DependentCode`) is invalidated while a function is waiting on the call stack -- V8 cannot continue executing the optimized machine code. Instead, V8 performs **Deoptimization**:
1. It pauses execution at the exact point of failure.
2. It reads compiler-generated metadata (`DeoptimizationData` and `DeoptimizationFrameTranslation`) to decode registers and stack slots.
3. It reconstructs the equivalent **unoptimized stack frames** (including multiple frames if functions were inlined) and materializes any scalar-replaced (escape-analyzed) heap objects.
4. It transfers control seamlessly into the `InterpreterEntryTrampoline` (`Builtin::kNotifyDeoptimized`), resuming execution in Ignition (or OSR) as if the optimized code had never existed.

---

## 1. Deoptimization Kinds: `kEager` vs `kLazy` (`src/common/globals.h`)

V8 defines two deoptimization kinds in `enum class DeoptimizeKind : uint8_t` (`src/common/globals.h`):

### 1. `DeoptimizeKind::kEager` (Synchronous Self-Check Failure)
Triggered **synchronously** by a guard check inside the currently executing optimized frame itself.
- **Examples**: Map check failure (`CheckMaps`), Smi check failure (`CheckSmi`), integer overflow (`CheckedInt32Add`), bounds check failure (`CheckBounds`), or hole check failure (`CheckNotHole`).
- **Mechanism**: The failed check jumps directly to a compiler-generated **deoptimization exit** stub at the end of the `InstructionStream`, which pushes the deoptimization exit ID and jumps to `Builtin::kDeoptimizationEntry_Eager`.
- **Frame State**: Reconstructs the unoptimized bytecode state **at or immediately before** the current bytecode instruction (using `INTERPRETED_FRAME_WITHOUT_RETURN`), so Ignition re-executes the operation using its generic slow path.

### 2. `DeoptimizeKind::kLazy` (Asynchronous On-Stack Invalidation)
Triggered **asynchronously** while an optimized activation is suspended on the call stack waiting for a callee (a JS call, runtime call, builtin call, or fast C API call) to return.
- **Examples**:
  - While function `f()` is paused waiting for `g()` to return, `g()` modifies a prototype, overwrites a `PropertyConstness::kConst` field, transitions a `ContextCell`, or deprecates a `Map` that `f()` depended on via `DependentCode`.
  - A debugger sets a breakpoint inside `f()`.
- **Mechanism (`MarkForDeoptimization` & Return Address Patching)**:
  - Because `f()` is not currently executing on the CPU, V8 cannot rewrite `f()`'s stack frame immediately.
  - Instead, V8 marks `f()`'s `Code` object `marked_for_deoptimization()` and walks the stack (or uses the code's deopt table / safepoint return PCs) to patch return flow so that when `g()` returns into `f()`, execution is redirected to `Builtin::kDeoptimizationEntry_Lazy` at the call site's lazy deopt point.
  - *(Note on Fast C Calls: Historically V8 had a separate `kLazyAfterFastCall` kind for V8 Fast API calls; in modern V8, fast C calls use standard `kLazy` deoptimization transitions when returning from the C++ callback if the caller frame was marked for deoptimization during the call.)*
- **Frame State**: Because the callee `g()` has **already finished executing** and produced a return value (stored in the return register `kReturnRegister0`), the reconstructed unoptimized frame for `f()` must resume **after** the call bytecode and place `g()`'s return value into the target bytecode register or accumulator (`INTERPRETED_FRAME_WITH_RETURN`).

---

## 2. Trusted Deoptimization Metadata (`src/objects/deoptimization-data.h`)

To protect the V8 Sandbox against attacker-controlled heap corruption, all deoptimization metadata lives in **Trusted Space** (`ProtectedFixedArray` and `TrustedByteArray`) attached to `Code::deoptimization_data()`.

```text
Code (TrustedSpace)
  |
  +-- deoptimization_data() --> DeoptimizationData (ProtectedFixedArray)
        |-- [0] FrameTranslation       (Tagged<DeoptimizationFrameTranslation> : TrustedByteArray)
        |-- [1] InlinedFunctionCount   (Smi)
        |-- [2] ProtectedLiteralArray  (Tagged<ProtectedDeoptimizationLiteralArray> : ProtectedFixedArray)
        |-- [3] LiteralArray           (Tagged<DeoptimizationLiteralArray> : TrustedWeakFixedArray)
        |-- [4] OsrBytecodeOffset      (Smi)
        |-- [5] OsrPcOffset            (Smi)
        |-- [6] OptimizationId         (Smi)
        |-- [7] WrappedSharedFunctionInfo (SharedFunctionInfoWrapperOrSmi)
        |-- [8] InliningPositions      (Tagged<TrustedPodArray<InliningPosition>>)
        |-- [9] DeoptExitStart         (Smi)
        |-- [10] EagerDeoptCount       (Smi)
        |-- [11] LazyDeoptCount        (Smi)
        +-- [12..] Deopt Entries       (Tuples of: BytecodeOffsetRaw, TranslationIndex, Pc [, NodeId])
```

### Why Two Literal Arrays? (`LiteralArray` vs `ProtectedLiteralArray`)
During frame reconstruction, translation opcodes frequently reference compile-time constants ("deoptimization literals") by index:
1. **`LiteralArray` (`DeoptimizationLiteralArray` : `TrustedWeakFixedArray`)**:
   - The table itself resides in trusted space, and its entries point to regular V8 heap objects inside the main pointer-compression cage (`SharedFunctionInfo`, `Map`, `HeapNumber`, `String`, etc.), held strongly or weakly according to `InstructionStream::IsWeakObjectInOptimizedCode`.
2. **`ProtectedLiteralArray` (`ProtectedDeoptimizationLiteralArray` : `ProtectedFixedArray`)**:
   - Holds pointers to **Trusted Space** objects -- most importantly **`BytecodeArray`** (and `InterpreterData` / `Code`) for the outer function and all inlined functions (`TranslationOpcode::INTERPRETED_FRAME_*` operand `bytecode_array_literal`).
   - Keeping `BytecodeArray` pointers in `ProtectedLiteralArray` guarantees that a sandbox attacker cannot corrupt a deoptimization literal to make the deoptimizer construct an `InterpretedFrame` pointing to a forged `BytecodeArray`.

---

## 3. Frame Translations (`DeoptimizationFrameTranslation` & `TranslationOpcode`)

Each deoptimization entry in `DeoptimizationData` contains a `TranslationIndex` pointing into the compressed byte stream of `DeoptimizationFrameTranslation` (`src/deoptimizer/frame-translation-builder.cc`, `src/deoptimizer/translation-opcode.h`).

A translation begins with a `BEGIN` opcode (`BEGIN_WITHOUT_FEEDBACK` or `BEGIN_WITH_FEEDBACK`, encoding `distance_to_previous_translation`, `frame_count`, and `jsframe_count`), followed by a sequence of **Frame Opcodes** (ordered outermost caller first, innermost inlined callee last) and **Value Opcodes** for each value in that frame.

> [!TIP]
> **Delta Compression (`MATCH_PREVIOUS_TRANSLATION`)**: Consecutive deopt points in a function usually have nearly identical frame and register layouts. `DeoptimizationFrameTranslationBuilder` compresses the byte stream by emitting `MATCH_PREVIOUS_TRANSLATION(repeat_count)`, instructing `DeoptTranslationIterator` to replay `repeat_count` opcodes from the preceding `BEGIN` entry!

### Key Frame Translation Opcodes (`src/deoptimizer/translation-opcode.h`)

| `TranslationOpcode` | Operands | Purpose |
| :--- | :---: | :--- |
| **`INTERPRETED_FRAME_WITH_RETURN`** | `6`: `bytecode_offset`, `shared_info_id`, `bytecode_array_id`, `height`, `return_value_offset`, `return_value_count` | Reconstructs an unoptimized `InterpretedFrame` for a **lazy deopt** (or inlined caller awaiting a return value), patching `return_value_count` return values at `return_value_offset` in the frame's register file. |
| **`INTERPRETED_FRAME_WITHOUT_RETURN`** | `4`: `bytecode_offset`, `shared_info_id`, `bytecode_array_id`, `height` | Reconstructs an `InterpretedFrame` where no callee return value is patched in (`return_value_offset = 0, return_value_count = 0`), such as an **eager deopt**. |
| **`CONSTRUCT_CREATE_STUB_FRAME`** / **`CONSTRUCT_INVOKE_STUB_FRAME`** | `2` / `1` | Reconstructs a `ConstructFrame` around an inlined `new Foo()` constructor call (either before or after the constructor invocation). |
| **`BUILTIN_CONTINUATION_FRAME`** / **`JAVASCRIPT_BUILTIN_CONTINUATION_FRAME`** / **`..._WITH_CATCH`** | `3` | Reconstructs a continuation frame when an inlined C++ or Torque builtin deoptimizes mid-execution (for example, `Array.prototype.map` callback deoptimizing). |
| **`INLINED_EXTRA_ARGUMENTS`** | `3` | Reconstructs under-applied or over-applied arguments for an inlined call so `arguments` / rest parameters inside the callee see the exact argument list. |

### Key Value Translation Opcodes

For each frame, the translation specifies how to materialize the closure, receiver, parameters, context, bytecode registers, and accumulator (`src/deoptimizer/translated-state.h`):
- **Physical Locations**: `REGISTER`, `INT32_REGISTER`, `INT64_REGISTER`, `UINT32_REGISTER`, `BOOL_REGISTER`, `FLOAT_REGISTER`, `DOUBLE_REGISTER`, `HOLEY_DOUBLE_REGISTER`, `SIMD128_REGISTER`, and their `*_STACK_SLOT` counterparts (`TAGGED_STACK_SLOT`, `INT32_STACK_SLOT`, etc.).
- **Literals**:
  - `LITERAL(literal_id)`: Reads a heap object from `DeoptimizationLiteralArray`.
  - `DOUBLE_LITERAL(low, high)` vs `HOLEY_DOUBLE_LITERAL(low, high)`: Encodes raw 64-bit `Float64` vs `HoleyFloat64` constants directly in the translation stream without laundering hole NaNs through `LITERAL(the_hole_value)`. For tagged slots (`GetRawValue()` / `GetValue()`), `DOUBLE_LITERAL` canonicalizes all NaNs (including `kHoleNanInt64`) to a quiet NaN `HeapNumber`, whereas `HOLEY_DOUBLE_LITERAL` converts `is_hole_nan()` to `undefined_value`; for `MaterializeFixedDoubleArray`, `GetDoubleValue()` writes the raw `Float64` bits via `FixedDoubleArray::set_raw` (see [Holes and Sentinels](../objects/holes-and-sentinels.md)).
- **Escape-Analyzed (Virtual) Objects**:
  - `CAPTURED_OBJECT(length)`: Describes a scalar-replaced object that was never allocated on the heap in optimized code! Followed by `length` field translations (the first being its `Map`).
  - `DUPLICATED_OBJECT(object_id)`: References a previously declared `CAPTURED_OBJECT` within the same translation (handling cyclic or shared references among virtual objects).
- **Special Sentinels**:
  - `ARGUMENTS_ELEMENTS(type)` / `ARGUMENTS_LENGTH`: Materializes an unallocated `arguments` backing store.
  - `REST_LENGTH`: Materializes rest parameter length.
  - `OPTIMIZED_OUT`: Places `ReadOnlyRoots::optimized_out()` into dead stack/register slots.
  - `UPDATE_FEEDBACK(vector_literal, slot)`: Instructs the deoptimizer to update a `FeedbackVector` slot (e.g. `SpeculationMode::kDisallowSpeculation`) so the compiler does not repeatedly make the same failed speculation on re-optimization!

---

## 4. The Four-Phase Execution Flow (`src/deoptimizer/deoptimizer.cc`)

```mermaid
sequenceDiagram
    participant Opt as Optimized Code (Maglev/TurboFan)
    participant Stub as Builtin::kDeoptimizationEntry
    participant Cpp as Deoptimizer (C++ no-GC)
    participant Notify as Builtin::kNotifyDeoptimized
    participant Ign as Ignition (InterpreterEntryTrampoline)

    Opt->>Stub: Jump to Deopt Exit (Eager) or Return to Patched PC (Lazy)
    Stub->>Stub: Spill all general & FP registers into FrameDescription (input_)
    Stub->>Cpp: Deoptimizer::New(raw_function, kind, deopt_exit_index, fp_to_sp)
    Cpp->>Cpp: 1. TranslatedState::Init() (decode translation & capture register/stack values)
    Cpp->>Cpp: 2. DoComputeOutputFrames() (build output_ FrameDescriptions)
    Cpp-->>Stub: Return to assembly stub
    Stub->>Stub: Pop input_ frame, copy output_ FrameDescriptions onto physical stack
    Stub->>Notify: Jump to Builtin::kNotifyDeoptimized
    Notify->>Cpp: Runtime_NotifyDeoptimized (GC allowed!)
    Cpp->>Cpp: 3. TranslatedState::MaterializeHeapObjects() (box numbers, allocate CAPTURED_OBJECTs)
    Notify->>Ign: 4. Resume in InterpreterEntryTrampoline (or Builtin Continuation)
```

### Why Materialization Happens in Two Stages (No-GC vs GC-Allowed)
A crucial design constraint of V8's `Deoptimizer` is that **Garbage Collection is strictly forbidden (`DisallowGarbageCollection`) while `Deoptimizer::DoComputeOutputFrames()` is rewriting the physical stack**.

What if the deoptimized frame contains unboxed `int32`/`float64` values that need `HeapNumber` boxes, or escape-analyzed `CAPTURED_OBJECT`s that must be allocated on the heap?
1. **Stage 1 (`DoComputeOutputFrames`, No-GC)**:
   - `TranslatedState::Init` reads raw values out of `input_` (`FrameDescription`) and records all required heap allocations inside `TranslatedState` (`TranslatedFrame` / `TranslatedValue` entries on the C++ heap).
   - For any slot that requires heap allocation (such as a boxed `HeapNumber` or a `CAPTURED_OBJECT`), `DoComputeOutputFrames` temporarily writes a safe sentinel (`ReadOnlyRoots::arguments_marker()`) into the output `FrameDescription` stack slot!
   - The assembly stub copies these GC-safe output frames onto the physical CPU stack and calls `Builtin::kNotifyDeoptimized` -> `Runtime_NotifyDeoptimized`.
2. **Stage 2 (`Runtime_NotifyDeoptimized`, GC Allowed)**:
   - Now that the physical CPU stack consists of valid, walkable `InterpretedFrame`s (with `arguments_marker` in unmaterialized slots), GC is safe!
   - `TranslatedState::DoMaterializeHeapObjects` allocates all `HeapNumber`s, `BigInt`s, `arguments` objects, and `CAPTURED_OBJECT` graphs on the V8 heap (carefully handling cycles and nested `Representation::Double()` fields) and patches their addresses directly into the reconstructed stack frame slots (`FrameSlotPointer`).

---

## Key Source Files

- `src/deoptimizer/deoptimizer.h` & `src/deoptimizer/deoptimizer.cc`: Core `Deoptimizer` orchestration and `DoComputeOutputFrames`.
- `src/deoptimizer/translated-state.h` & `src/deoptimizer/translated-state.cc`: `TranslatedState`, `TranslatedFrame`, `TranslatedValue`, and escape-analyzed object materialization.
- `src/deoptimizer/translation-opcode.h`: Complete list of `TranslationOpcode` frame and value opcodes.
- `src/deoptimizer/frame-translation-builder.h` & `.cc`: Compressing and emitting `DeoptimizationFrameTranslation` byte streams.
- `src/objects/deoptimization-data.h` & `-inl.h`: `DeoptimizationData`, `DeoptimizationLiteralArray`, and `ProtectedDeoptimizationLiteralArray`.
