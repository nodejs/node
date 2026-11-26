# Turboshaft Intermediate Representation (IR)

Turboshaft's IR (`src/compiler/turboshaft/graph.h`, `index.h`, `operations.h`, `representations.h`) is an explicit, flat-memory Static Single Assignment (SSA) Control-Flow Graph (CFG). Unlike node-pointer-based IRs where every instruction is a heap-allocated node connected by pointer arrays, Turboshaft stores variable-length operations inline inside a contiguous buffer and references them via 32-bit byte offsets (`OpIndex`) wrapped in compile-time typed handles (`V<T>`).

---

## 1. Conceptual Overview: `Graph`, `Block`, and `OperationBuffer`

A Turboshaft **`Graph`** (`src/compiler/turboshaft/graph.h`) consists of two main structures:

```
  Block 0 (kMerge)               Block 1 (kLoopHeader)
  [begin: 0, end: 48)            [begin: 48, end: 112)
       |                              |
       v                              v
  +----+------------+-----------------+-------------+---------------+
  | ParameterOp     | ConstantOp      | PhiOp       | WordBinopOp   | ...
  | (16 bytes)      | (16 bytes)      | (24 bytes)  | (24 bytes)    |
  +-----------------+-----------------+-------------+---------------+
  ^ offset 0        ^ offset 16       ^ offset 48   ^ offset 72
    (id 0)            (id 1)            (id 3)        (id 4)
                     OperationBuffer (8-byte OperationStorageSlots)
```

1. **`OperationBuffer` (`src/compiler/turboshaft/graph.h`)**:
   An append-only, Zone-allocated array of 8-byte **`OperationStorageSlot` (`uint64_t`)** elements. Every operation in the graph is constructed in-place inside this contiguous buffer. Because every operation occupies at least 2 slots (`kSlotsPerId = 2`, i.e., 16 bytes), a parallel `uint16_t* operation_sizes_` array stores the slot count at both the first and last `id` of each operation, enabling `O(1)` forward (`Next(OpIndex)`) and backward (`Previous(OpIndex)`) iteration across variable-length operations.
   * **In-Place Replacement (`OperationBuffer::ReplaceScope`)**: Although the buffer is append-only during normal emission, `Graph::Replace<NewOp>(OpIndex replaced, ...)` uses `ReplaceScope` to temporarily rewind `end_` and overwrite an existing operation in place (most commonly to replace a 1-input `PendingLoopPhiOp` with a 2-input `PhiOp`), asserting `DCHECK_LE(new_slot_count, old_slot_count_)`.
2. **`Block` (`src/compiler/turboshaft/graph.h`)**:
   Represents a basic block (`Block::Kind::kMerge`, `kLoopHeader`, or `kBranchTarget`). A `Block` does not store a vector of operation pointers; it simply stores `OpIndex begin_` and `OpIndex end_` delimiting a contiguous slice of the `OperationBuffer`.
   * Each `Block` also inherits from **`RandomAccessStackDominatorNode<Block>`**, which maintains the dominator tree using Myers' random-access stack (`nxt_`, `jmp_`, `len_`), supporting `O(log h)` Lowest Common Dominator queries (`GetCommonDominator`).

---

## 2. `OpIndex` and Debug Generation Bit Checking

An **`OpIndex`** (`src/compiler/turboshaft/index.h`) is a lightweight 32-bit handle (`uint32_t offset_`) representing an operation's byte offset from the start of `OperationBuffer`. Compared to raw `Operation*` pointers:
* It is 32 bits instead of 64 bits (halving memory usage in inputs and side tables).
* It remains valid when `OperationBuffer::Grow()` reallocates the underlying buffer.
* Its compact **`id()`** (`offset_ / sizeof(OperationStorageSlot) / kSlotsPerId`, i.e., `offset_ / 16`) directly indexes into dense side tables (`FixedOpIndexSidetable<T>`, `GrowingOpIndexSidetable<T>`).

### The `generation_mod2` Bit (`#ifdef DEBUG`)
Because Turboshaft's `CopyingPhase` reads from an `input_graph` and emits into a companion `output_graph`, a classic compiler bug is accidentally passing an unmapped `OpIndex` from the `input_graph` into an operation emitted into the `output_graph` (or indexing a side table with an `OpIndex` from the wrong graph).

To catch cross-graph `OpIndex` bugs immediately in debug builds, `OpIndex` exploits alignment bits in `offset_`:
* Because `sizeof(OperationStorageSlot) == 8`, every valid byte offset in `OperationBuffer` is a multiple of 8, meaning the lowest 3 bits (`0b111`) of the raw byte offset are always zero.
* **Bit 0 (`kExternalIdFlag = 1`)** is reserved for `EncodeExternalId` (encoding Sea-of-Nodes or Maglev node IDs in `NodeOriginTable`).
* **Bit 1 (`kGenerationMask = 1 << 1`)** stores **`generation_mod2()`** in `#ifdef DEBUG`:
  * Each `Graph` tracks a `generation_` counter (`graph_generation_ % 2`), and its companion output graph always has `(generation_ + 1) % 2`.
  * When `OperationBuffer` creates an `OpIndex` in `Graph::Allocate`, it stamps the graph's `generation_mod2` bit into bit 1 of `offset_`.
  * `OpIndex::offset()` masks out bit 1 (`offset_ & kUnmaskGenerationMask`) when indexing memory, while `Graph::BelongsToThisGraph(OpIndex idx)` and `Sidetable::operator[]` assert `DCHECK_EQ(idx.generation_mod2(), generation_mod2())`.
  * Any attempt to use an `input_graph` `OpIndex` directly in the `output_graph` without calling `MapToNewGraph` triggers an immediate `DCHECK` failure.

---

## 3. Compile-Time Typed SSA Values: `V<T>`, `OptionalV<T>`, and `ConstOrV<T>`

While `OpIndex` is untyped, Turboshaft reducers and operations almost always use **`V<T>`** (`src/compiler/turboshaft/index.h`), a zero-overhead template subclass of `OpIndex` parameterized by a type tag `T`.

### Type Tags and `v_traits<T>`
`T` in `V<T>` can be:
1. **Abstract Machine Representation Tags**:
   * `Word32`, `Word64`, `WordPtr` (alias for `Word64` on 64-bit targets or `Word32` on 32-bit targets)
   * `Float32`, `Float64`
   * `Simd128`, `Simd256`, `Compressed`
   * `Any` (accepts any representation), `None` (for operations with no value output, such as `StoreOp`, `GotoOp`, `BranchOp`, or `DeoptimizeOp`)
   * Internal tags: `EagerFrameState`, `LazyFrameState`, `AnyFrameState` (`UntaggedUnion<EagerFrameState, LazyFrameState>`)
   * Multi-output tuples: `Tuple<T1, T2, ...>` (e.g., `Word32Pair = Tuple<Word32, Word32>`)
   * Untagged unions: `UntaggedUnion<Ts...>` (e.g., `Word = UntaggedUnion<Word32, Word64>`, `Float = UntaggedUnion<Float32, Float64>`, `Untagged = UntaggedUnion<Word, Float>`)
2. **V8 Heap Object Hierarchy Types (`RegisterRepresentation::Tagged()`)**:
   * Any V8 `Object` subtype (`Object`, `Smi`, `HeapObject`, `Number`, `String`, `Symbol`, `Boolean`, `JSReceiver`, `JSObject`, `JSFunction`, `Map`, `Context`, `FixedArray`, etc.)
   * Tagged unions: `Union<T, Ts...>` / `UnionOf<...>` (e.g., `NumberOrString`, `PlainPrimitive`, `BooleanOrNullOrUndefined`, `AnyFixedArray`)

For every type tag `T`, **`v_traits<T>`** defines:
* `v_traits<T>::rep`: The static `RegisterRepresentation` (`Word32()`, `Word64()`, `Float32()`, `Float64()`, `Tagged()`, `Compressed()`, `Simd128()`, or `nullrep` if polymorphic like `Any` or `Word`).
* `v_traits<T>::implicitly_constructible_from<U>`: Controls implicit upcasting (`V<Smi>` or `V<JSFunction>` implicitly converts to `V<Object>` or `V<Any>`, and `V<Word32>` implicitly converts to `V<Word>` or `V<Untagged>`).
* Downcasting requires `V<Target>::Cast(v)` (which `static_assert`s that the cast is not a redundant upcast and forbids casting between `EagerFrameState` and `LazyFrameState`) or `V<Target>::CastIfNeeded(v)` in generic template code.

### `OptionalV<T>`
`OptionalV<T>` inherits from `OptionalOpIndex` and represents an optional SSA value (for example, an optional `OptionalV<LazyFrameState>` on `CallOp` or an optional base/index on `StoreOp`).
* **Unreachable Code Relaxation (`detail::generating_unreachable_operations()`)**: When an assembler is in unreachable code (`current_block() == nullptr`), emitting an operation returns `OpIndex::Invalid()`. To prevent callers from crashing when calling `.value()` on an `OptionalV<T>` right after emitting an operation in unreachable code, `OptionalV<T>::value()` checks `DCHECK(has_value() || detail::generating_unreachable_operations())` using the thread-local `detail::current_assembler_block` pointer.

### `ConstOrV<T>`
`ConstOrV<T>` (`src/compiler/turboshaft/index.h`) is a union of a compile-time C++ constant (`v_traits<T>::constexpr_type`, such as `uint32_t` for `Word32`, `uint64_t` for `Word64`, `double` for `Float64`, or `Tagged<Smi>` for `Smi`) and a `V<T>`.
* It allows reducer authors to write natural expressions with immediate literals, such as:
  ```cpp
  V<Word32> result = __ Word32Add(x, 1);
  ```
  instead of `__ Word32Add(x, __ Word32Constant(1))`.
* Inside `AssemblerOpInterface`, helper methods call `Asm().resolve(const_or_v)` to materialize a `ConstantOp` on demand if `const_or_v.is_constant()` is true, or extract `const_or_v.value()` otherwise. Attempting to implicitly convert an unresolved `ConstOrV<T>` directly into an `OpIndex` triggers a descriptive `static_assert` error reminding the developer to call `resolve()`.

---

## 4. Operation Structure, `Explode`, and `OpEffects`

### Anatomy of a `FooOp` (`src/compiler/turboshaft/operations.h`)
Every Turboshaft operation `Foo` is listed in `TURBOSHAFT_OPERATION_LIST(V)` (which generates `Opcode::kFoo`) and defined as a `struct FooOp` inheriting from either:
* **`FixedArityOperationT<kInputCount, FooOp>`**: For operations with a fixed compile-time number of inputs (e.g., `WordBinopOp` has `FixedArityOperationT<2, WordBinopOp>`). Stores its `OpIndex` inputs inline in a `std::array<OpIndex, kInputCount> inputs_`.
* **`OperationT<FooOp>`**: For variable-arity operations (e.g., `PhiOp`, `CallOp`, `FrameStateOp`, `MakeTupleOp`). Allocates trailing storage slots in `OperationBuffer` right after the `FooOp` struct fields to hold its `input_count` `OpIndex` values.

Each `FooOp` defines:
1. **Inputs**: Accessed via `inputs()` (`base::Vector<const OpIndex>`) and named typed getters (e.g., `V<Word> left() const { return input<Word>(0); }`).
2. **Options**: Non-SSA compile-time parameters (such as `Kind`, `WordRepresentation`, `FeedbackSource`, or `WriteBarrierKind`) stored as struct fields and exposed as a `std::tuple` via **`options() const`**. Default hashing (`hash_value()`), equality comparison (`operator==`), and printing (`PrintOptions`) automatically operate over `inputs()` and `options()`.
3. **`Explode(fn, mapper)`**:
   Unpacks the operation's inputs (mapping each input through `mapper.Map(...)`) followed by its `options()` tuple elements, and invokes `fn(mapped_inputs..., options...)`.
   * `FixedArityOperationT<N, Derived>::Explode` provides this automatically via `std::index_sequence` over `inputs_` and `options()`.
   * During `CopyingPhase`, `OutputGraphAssembler::AssembleOutputGraphFoo(const FooOp& op)` simply calls:
     ```cpp
     op.Explode([a = assembler()](auto... args) {
       return a->ReduceFoo(args...);
     }, *this);
     ```
     which maps every `OpIndex` input from the old graph to the new graph via `MapToNewGraph` and forwards the unpacked arguments and options straight to `ReduceFoo(...)`.
4. **`OpEffects` (`src/compiler/turboshaft/operations.h`)**:
   Every operation specifies its side effects and dependencies via a 16-bit **`OpEffects`** bitfield (`EffectDimensions produces`, `EffectDimensions consumes`, `can_create_identity`, `can_allocate`, `required_when_unused`). The 7 `EffectDimensions` bits track:
   * `load_heap_memory`, `load_off_heap_memory`
   * `store_heap_memory`, `store_off_heap_memory`
   * `before_raw_heap_access`, `after_raw_heap_access`
   * `control_flow`
   Pure operations declare `OpEffects().CanDependOnChecks()` or `OpEffects()` (all zero bits), allowing `ValueNumberingReducer` (GVN) and `InstructionSelector::CanCover` to reason uniformly about effect interference.

---

## 5. Throwing Operations & `THROWING_OP_BOILERPLATE`

In a CFG IR, an operation that may throw a JavaScript exception (and be caught by an in-frame `try`/`catch` handler or lazy-deopt on throw) has both value outputs (on the non-throwing path) and control-flow edges (branching to the normal continuation block vs. the `catch` block).

Turboshaft decomposes every potentially throwing operation (`MayThrow(Opcode)` in `src/compiler/turboshaft/operations.h`: `CallOp`, `FastApiCallOp`, `GenericBinopOp`, `GenericUnopOp`, `ToNumberOrNumericOp`, `LoadDictionaryFieldOp`, `StringLocaleCompareIntlOp`, `StringToCaseIntlOp`) into a standardized 3-part pattern in the CFG:

```
  [ Current Block ]
    ...
    o10: GenericBinopOp(lhs, rhs, frame_state, ...)   <-- outputs_rep() is empty!
    o11: CheckExceptionOp(o10, didnt_throw_block: B1, catch_block: B2)
          |                                   |
          v (normal path: B1)                 v (exception path: B2)
  [ Block B1 ]                        [ Block B2 ]
    o12: DidntThrowOp(o10)              o20: CatchBlockBeginOp()
    (produces actual outputs!)          ...
```

1. **The Throwing Operation itself (`o10`)**:
   Has **`outputs_rep() == {}`** (empty) because its result does not exist along the exceptional edge. No consumer is allowed to reference `o10` as a value input directly; only `CheckExceptionOp` and `DidntThrowOp` reference `o10`.
2. **`CheckExceptionOp` (Block Terminator, `o11`)**:
   Terminates the block immediately after a throwing operation is emitted inside a `CatchScope` (if inside a `try` block). Connects control flow to `didnt_throw_block` and `catch_block`.
3. **`DidntThrowOp` (`o12`, first operation of `didnt_throw_block`)**:
   References `throwing_operation()` (`o10`) and **produces the actual SSA value outputs** of `o10` on the non-throwing path. Consumers of the call/binop result use `o12`'s `OpIndex`.

### `THROWING_OP_BOILERPLATE` (`src/compiler/turboshaft/operations.h`)
For operations in `TURBOSHAFT_JS_THROWING_OPERATION_LIST` whose output representations are statically known, the **`THROWING_OP_BOILERPLATE(...)`** macro generates the required plumbing:

```cpp
// src/compiler/turboshaft/operations.h
#define THROWING_OP_BOILERPLATE(...)                                         \
  static constexpr RegisterRepresentation kOutputRepsStorage[]{__VA_ARGS__}; \
  static constexpr base::Vector<const RegisterRepresentation> kOutReps =     \
      base::VectorOf(kOutputRepsStorage, arraysize(kOutputRepsStorage));     \
  base::Vector<const RegisterRepresentation> outputs_rep() const {           \
    return {};                                                               \
  }                                                                          \
  LazyDeoptOnThrow lazy_deopt_on_throw;
```

* It defines `outputs_rep()` on the throwing operation itself to return `{}` (empty), while exposing `kOutReps` so the companion `DidntThrowOp` knows the output representation(s) to expose on the non-throwing path.
* It adds a **`LazyDeoptOnThrow lazy_deopt_on_throw`** field (which must be included in the operation's `options()` tuple). When Turbolev compiles a `try`/`catch` block whose `catch` handler has never been entered at runtime, it omits the `catch` block and sets `lazy_deopt_on_throw = LazyDeoptOnThrow::kYes` so the operation lazily deoptimizes if an exception is thrown for the first time.
