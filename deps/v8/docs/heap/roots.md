# Roots in V8 (`RootsTable` & `ReadOnlyRoots`)

In V8, **Roots** are fundamental heap values (`undefined`, `null`, `true`, `false`, `the_hole`, core `Map`s like `meta_map` and `fixed_array_map`, canonical empty arrays, symbol tables, and builtin objects) that are always reachable and live at fixed, well-known indices (`RootIndex`) in the **`RootsTable`** (`src/roots/roots.h`).

---

## 1. `RootsTable` vs. `ReadOnlyRoots` (`src/roots/roots.h`, `src/roots/roots-inl.h`)

V8 provides two distinct C++ interfaces for accessing roots depending on whether the root is **mutable** or **read-only**:

| Facility | Accessor Return Type | Backing Mechanism | Typical Usage |
| :--- | :--- | :--- | :--- |
| **`RootsTable` / `Isolate::factory()`** | **`IndirectHandle<T>`** (`Handle<T>`) or `IndirectHandle<ReadOnly<T>>` | Returns a handle wrapping `&isolate->roots_table()[root_index]`, pointing directly to the `Address` slot inside `IsolateData`. | Passing a root to an API that specifically requires an `IndirectHandle<T>` (`Handle<T>`), or accessing **mutable** per-Isolate roots (e.g., `string_table`, `empty_script`). |
| **`ReadOnlyRoots`** | **`Tagged<ReadOnly<T>>`** (returned **by value**) | Returns a raw `Tagged<ReadOnly<T>>` with zero handle indirection. Under `V8_STATIC_ROOTS_BOOL`, adds a compile-time 32-bit constant offset directly to the cage base without any memory load. | Fast comparisons (`obj == ReadOnlyRoots(isolate).undefined_value()`), initializing new objects (`set_map`), and background-thread access where no `HandleScope` is needed. |

### Why `RootsTable` Returns `IndirectHandle<T>` Without a `HandleScope`
`IsolateData` embeds the contiguous array `Address roots_[RootIndex::kEntriesCount]` directly inside the `Isolate`. Because every entry in `roots_` is scanned by the GC as a root and remains at a fixed C++ address for the entire lifetime of the `Isolate`, `isolate->root_handle(index)` (and `factory()->undefined_value()`, etc.) simply constructs an `IndirectHandle<T>(&roots_table_[index])` without allocating a slot in any `HandleScope`.

### Why `ReadOnlyRoots` Returns `Tagged<ReadOnly<T>>` By Value
Every root in `READ_ONLY_ROOT_LIST` resides in `ReadOnlySpace`: it is **immortal** (never collected) and **immovable** (never evacuated by compaction). Therefore:
1. Holding a raw `Tagged<ReadOnly<T>>` across a garbage collection is completely safe (`gcmole` and the C++ type system know `ReadOnly<T>` cannot move).
2. Storing a `Tagged<ReadOnly<T>>` into a heap object requires **no write barrier** (`SKIP_WRITE_BARRIER` is safe by construction because `ReadOnlySpace` objects are never young, never evacuated, and never marked).
3. Converting a `Tagged<ReadOnly<T>>` to a `DirectHandle<T>` on the stack is free.

---

## 2. Static Read-Only Roots (`V8_STATIC_ROOTS_BOOL` in `src/roots/static-roots.h`)

On 64-bit builds with pointer compression in a shared cage (`V8_STATIC_ROOTS_BOOL == 1`), `ReadOnlySpace` is laid out deterministically at the very start of the 4GB pointer compression cage during `mksnapshot`. Every read-only root therefore has a **compile-time constant 32-bit compressed address** (`StaticReadOnlyRoot::kUndefinedValue`, `StaticReadOnlyRoot::kNullValue`, `StaticReadOnlyRoot::kMetaMap`, etc., generated in `src/roots/static-roots.h`).

When `V8_STATIC_ROOTS_BOOL` is enabled (`src/roots/roots-inl.h`):
* **`ReadOnlyRoots::address_at(RootIndex)`** does **not** load from `read_only_roots_[index]` in memory. Instead, it looks up `StaticReadOnlyRootsPointerTable[index]` and decompresses the compile-time 32-bit `Tagged_t` by adding the cage base register/base:
  ```cpp
  V8_INLINE Address ReadOnlyRoots::address_at(RootIndex root_index) const {
  #if V8_STATIC_ROOTS_BOOL
    return V8HeapCompressionScheme::DecompressTagged(
        StaticReadOnlyRootsPointerTable[static_cast<int>(root_index)]);
  #else
    return read_only_roots_[static_cast<size_t>(root_index)];
  #endif
  }
  ```
* **`V8_RO_CONST` (`__attribute__((const))`)**: All `ReadOnlyRoots` getters are annotated with `V8_RO_CONST`, telling Clang that they are pure functions with no memory side effects so the compiler can hoist and common-subexpression-eliminate root lookups across function calls and loop iterations.
* **Single-Instruction Compare in JIT Code**: In baseline and optimizing compilers (Sparkplug, Maglev, TurboFan/Turboshaft), comparing a compressed `Tagged_t` value against `undefined`, `null`, `true`, `false`, or `the_hole` compiles to a single 32-bit immediate comparison (`cmp eax, kUndefinedValueRoot`) without reading `kRootRegister` or decompressing the pointer.

---

## 3. Categories of Roots in `RootsTable` (`src/roots/roots.h`)

The `ROOT_LIST` macro in `src/roots/roots.h` orders `RootIndex` entries into four contiguous regions so the GC can visit only the relevant subset during different collection phases:

1. **`READ_ONLY_ROOT_LIST` (`kFirstReadOnlyRoot .. kLastReadOnlyRoot`)**:
   * Shared across all `Isolate`s via `ReadOnlyHeap`.
   * Includes `undefined_value`, `the_hole_value`, `null_value`, `true_value`, `false_value`, `empty_string`, `meta_map`, core internal `Map`s (`fixed_array_map`, `heap_number_map`, `symbol_map`, `seq_two_byte_string_map`, etc.), `empty_fixed_array`, `empty_byte_array`, `empty_property_dictionary`, and specialized hole singletons (`property_cell_hole_value`, `hash_table_hole_value`, `uninitialized_value`, `arguments_marker`, `termination_exception`, `exception`).
   * **GC Behavior**: Never scanned or updated during GC (immortal and immutable).
2. **`STRONG_MUTABLE_IMMOVABLE_ROOT_LIST`**:
   * Per-Isolate objects allocated in non-moving spaces (or pinned in old space) whose contents or pointers may change over the lifetime of the `Isolate`, such as `builtins_constants_table`, `interpreter_entry_trampoline_for_profiling`, and canonical non-RO tables.
3. **`STRONG_MUTABLE_MOVABLE_ROOT_LIST`**:
   * Per-Isolate heap pointers that can be updated when the GC evacuates or replaces the target object, such as `string_table`, `api_private_symbol_table`, `public_symbol_table`, `script_list`, `materialized_objects`, `microtask_queue`, and `detached_contexts`.
   * **GC Behavior**: Scanned as strong roots during Minor and Major GC; slots are updated if their target objects are evacuated.
4. **`SMI_ROOT_LIST`**:
   * Root slots that always hold `Tagged<Smi>` values (such as `last_script_id`, `last_debugging_id`, `construct_stub_create_deopt_pc_offset`). Ignored by GC object visitors.

---

## 4. Root Register (`kRootRegister`) in Generated Code

During execution of JIT-compiled JavaScript, WebAssembly, and CSA/Torque builtins, V8 pins a dedicated machine register (`kRootRegister`) to point into the active `Isolate`'s `IsolateData` (`Isolate::isolate_root()`):
* **x64 (`src/codegen/x64/register-x64.h`)**: `r13` (`kRootRegister`), paired with `r14` (`kPtrComprCageBaseRegister`).
* **ARM64 (`src/codegen/arm64/register-arm64.h`)**: `x26` (`kRootRegister` / `rr`), paired with `x28` (`kPtrComprCageBaseRegister`).

`IsolateData` biases `isolate_root()` by `kRootRegisterBias` so both positive and negative compact signed displacements (`[kRootRegister + offset]`) can directly address:
* Every entry in `RootsTable` (`IsolateData::root_slot_offset(RootIndex)`),
* The Builtins entry table (`IsolateData::builtin_slot_offset(Builtin)`),
* External reference tables and thread-local stack limit / safepoint flags.
