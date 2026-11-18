# Handles and GC Safety Scopes in V8

Because V8's garbage collector moves `HeapObject`s during young-generation scavenging and old-generation compaction, raw `Tagged<T>` pointers in C++ local variables become invalid if a garbage collection occurs during their lifetime. **Handles** (`src/handles/`) keep referenced objects alive as GC roots and ensure C++ code observes their updated addresses after any relocation.

---

## 1. `IndirectHandle<T>` (`Handle<T>`) vs. `DirectHandle<T>` (`src/handles/handles.h`)

V8 provides two distinct local handle types in `src/handles/handles.h`:

> [!IMPORTANT]
> **`Handle<T>` is an alias for `IndirectHandle<T>`, NOT `DirectHandle<T>`** (`using Handle = IndirectHandle<T>;` in `src/handles/handles.h`).
> `DirectHandle<T>` is a separate C++ type that holds a direct stack word when `V8_ENABLE_DIRECT_HANDLE` is enabled (with Conservative Stack Scanning), or wraps an `IndirectHandle<T>` on builds where `V8_ENABLE_DIRECT_HANDLE` is disabled.

```
IndirectHandle<T> (alias Handle<T>)               DirectHandle<T> (with V8_ENABLE_DIRECT_HANDLE)
+-------------------+    HandleScope Block        +-----------------------------------+
| Address* location_|--->+-------------------+    | Address obj_ (on C++ machine stack|
+-------------------+    | Tagged HeapObject |    | scanned directly by CSS)          |
  (Stack Variable)       +-------------------+    +-----------------------------------+
                                   |                                |
                                   +---------------+----------------+
                                                   |
                                                   v
                                          +-----------------+
                                          |   HeapObject    |
                                          +-----------------+
```

### 1. `IndirectHandle<T>` (aliased as `Handle<T>`)
* **Representation**: Wraps a single pointer `Address* location_` pointing to a slot inside a heap/isolate-managed **`HandleScopeData`** block (`src/handles/handles.h`).
* **GC Behavior**: During a GC pause, the collector iterates all active `HandleScope` blocks, marks the pointed-to `HeapObject`s, and updates the `Address` inside each `HandleScope` slot if the object moves. Every `Handle<T>` sharing that `location_` automatically observes the new address.
* **Dereferencing**: Requires a double dereference (`Tagged<T>(*location_)`).
* **Where Required**: Must be used whenever a handle is stored on the C++ heap, inside a struct/class allocated off-stack, or passed across thread boundaries via `PersistentHandles`.

### 2. `DirectHandle<T>` (`src/handles/direct-handles.h`)
* **Representation**:
  * **When `V8_ENABLE_DIRECT_HANDLE` is enabled** (64-bit builds with Conservative Stack Scanning): Contains a single `Address obj_` holding the tagged value directly in the stack variable or CPU register. Allocating or dereferencing a `DirectHandle<T>` does **not** touch `HandleScopeData` and has zero indirection overhead.
  * **When `V8_ENABLE_DIRECT_HANDLE` is disabled**: Wraps an `IndirectHandle<T> handle_` transparently so the same C++ source code compiles in both configurations.
* **GC Behavior (Conservative Stack Scanning)**: When a GC occurs while `V8_ENABLE_DIRECT_HANDLE` is active, Conservative Stack Scanning (CSS) scans the C++ machine stack and saved registers. Any stack word looking like a pointer into a V8 page pins the target object (or quarantines its `NewSpace` page) so its address remains valid without slot indirection.
* **Stack-Only Enforcement (`StackAllocated`)**: `DirectHandle<T>` inherits from `v8::api_internal::StackAllocated<true>`, which deletes `operator new` and enforces via a Clang plugin that `DirectHandle<T>` is **only** declared on the C++ stack (never as a member of a heap-allocated class or inside a standard `std::vector`).
* **Interoperability**: Both `IndirectHandle<T>` and `DirectHandle<T>` implicitly convert to each other when needed (`DirectHandle<T>` reads `*indirect.location()`, while converting a `DirectHandle<T>` to an `IndirectHandle<T>` allocates a `HandleScope` slot). New C++ code should prefer `DirectHandle<T>` for stack variables and function parameters.

### 3. `MaybeDirectHandle<T>` and `MaybeHandle<T>` (`src/handles/maybe-handles.h`)
* Represent nullable handles (analogous to `std::optional<DirectHandle<T>>` / `std::optional<Handle<T>>`), typically used as return types of functions that may fail with a pending JavaScript exception.
* Callers must explicitly extract the handle via `.ToHandle(&out)` or `.ToHandleChecked()` before dereferencing.

### 4. `DirectHandleVector<T>` and `DirectHandleSmallVector<T, N>` (`src/handles/handles.h`)
Because `DirectHandle<T>` is stack-only and cannot be placed inside `std::vector<DirectHandle<T>>` (Conservative Stack Scanning does not scan `malloc`-allocated heap buffers), V8 provides dedicated container types:
* **`DirectHandleVector<T>`**: Stores `internal::DirectHandleUnchecked<T>` elements inside a buffer allocated via **`StrongRootAllocator<element_type>(isolate)`**. `StrongRootAllocator` registers the backing buffer with the `Heap` as a strong root range so the GC traces and updates every element in the vector even when the backing buffer grows on the C++ heap.
* **`DirectHandleSmallVector<T, kSize>`**: Inherits from `base::SmallVector<DirectHandleUnchecked<T>, kSize, StrongRootAllocator<...>>`, keeping up to `kSize` handles inline on the stack and falling back to `StrongRootAllocator` on the heap if resized beyond `kSize`.

---

## 2. `HandleScope` and `SealHandleScope`

### `HandleScope` (`src/handles/handles.h`)
* Manages the lifetime of `IndirectHandle<T>` slots on the main thread (`Isolate::handle_scope_data()`).
* Slots are bump-allocated inside 4KB `Address` blocks (`kHandleBlockSize`). When a `HandleScope` goes out of C++ scope, its destructor resets `HandleScopeData::next` and `limit` back to the saved state from when the `HandleScope` was entered, reclaiming all indirect handle slots created inside that scope in `O(1)` time.
* **`HandleScope::CloseAndEscape(handle)`**: Copies a single handle's value into the enclosing outer `HandleScope` before destroying the inner `HandleScope`, allowing a function to allocate temporary handles internally and return a single result handle without leaking slots.

### `SealHandleScope` (`src/handles/handles.h`)
* In `DEBUG` builds, `SealHandleScope` temporarily sets `HandleScopeData::level == HandleScopeData::sealed_level`.
* Any attempt to allocate a new `IndirectHandle` while a `SealHandleScope` is active triggers a `DCHECK` failure unless a new inner `HandleScope` is explicitly opened first. This catches accidental handle leaks inside hot loops or helper functions.

---

## 3. Off-Thread, Persistent, Global, and Traced Handles

| Handle Facility | Header | Threading & Lifetime | Purpose |
| :--- | :--- | :--- | :--- |
| **`LocalHandles` & `LocalHandleScope`** | `src/handles/local-handles.h` | Bound to a background thread's `LocalHeap` / `LocalIsolate`. | Allows background compiler and deserializer threads to create scoped handles scanned during safepoints (`LocalHeap::handles()`). |
| **`PersistentHandles` & `PersistentHandlesScope`** | `src/handles/persistent-handles.h` | Thread-safe detached handle blocks (`PersistentHandlesList`). | Created on the main thread (via `PersistentHandlesScope::Detach()`), passed to a background compilation job (e.g., TurboFan / Maglev), and destroyed when the job finishes. |
| **`GlobalHandles`** | `src/handles/global-handles.h` | Explicit lifetime (`Create` / `Destroy`), independent of `HandleScope`. | Backs `v8::Persistent<T>` and `v8::Global<T>`. Supports strong references and weak references with finalization callbacks (`WeakCallbackInfo`) or cleared slots. |
| **`EternalHandles`** | `src/handles/global-handles.h` | Immortal (lasts until `Isolate` teardown). | Backs `v8::Eternal<T>`; cheaper than `GlobalHandles` because slots are never freed or weak-processed. |
| **`TracedHandles` & `TracedNode`** | `src/handles/traced-handles.h` | Managed jointly by V8 and Oilpan (`CppHeap`). | Backs `v8::TracedReference<T>` for Unified Heap C++ wrappers. Tracks young-generation nodes (`is_in_young_list`), markbits, and droppable weak references. |

---

## 4. GC Safety Scopes and Static Analysis (`src/common/assert-scope.h`)

Whenever C++ code holds raw `Tagged<T>` pointers or iterates internal data structures that would be invalidated by a garbage collection, V8 uses RAII assertion scopes (`src/common/assert-scope.h`) and the **`gcmole`** Clang plugin (`tools/gcmole/gcmole.cc`) to guarantee no allocation or GC can occur.

### Runtime & Static Assertion Scopes (`src/common/assert-scope.h`)
* **`DisallowGarbageCollection`** (and class member macro **`DISALLOW_GARBAGE_COLLECTION(no_gc_)`**):
  * Combines `DisallowHeapAllocation` and `DisallowSafepoints` in debug builds (`PerThreadAssertScopeDebugOnly<SAFEPOINTS_ASSERT, HEAP_ALLOCATION_ASSERT, false>`).
  * Any attempt to allocate on the V8 heap or enter a safepoint while `DisallowGarbageCollection` is active triggers an immediate `DCHECK` crash.
  * Recognized statically by **`gcmole`** as a GC guard scope (`IsGCGuard`).
  * Functions that require callers to prove GC cannot happen accept `const DisallowGarbageCollection& no_gc` by reference (e.g., `HeapObject::GetWriteBarrierMode(const DisallowGarbageCollection&)` or `AllocationWitness`).
* **`DisallowGarbageCollectionInRelease`**: Enforces the same assertion even in release builds (`PerThreadAssertScope<..., false>`).
* **`AllowGarbageCollection`**: Temporarily overrides an enclosing `DisallowGarbageCollection` scope inside debug/test code where an allocation is intentionally safe.
* **`DisallowHandleAllocation`**: Forbids creating new `IndirectHandle`s on the current thread.
* **`DisallowHandleDereference`**: Forbids dereferencing handles (used on background compiler threads when reading heap state directly is unsafe without `HeapBroker`).
* **`DisallowHeapAccess`**: Combines `DisallowGarbageCollection`, `DisallowHandleAllocation`, and `DisallowHandleDereference` to completely isolate a background thread from the V8 heap.

### `HeapObject` Methods and `gcmole` (`NEVER_EVER_ALLOCATE_IN_OBJECT_METHOD`)
Inside any non-static member method of a `HeapObject` subclass (`V8_OBJECT class Foo : public HeapObject`), the C++ `this` pointer is a **raw, unhandlified address** pointing directly into the V8 heap.
* **Rule**: **Never allocate or call any function that may trigger GC inside a non-static `HeapObject` member method** (historically enforced by the `NEVER_EVER_ALLOCATE_IN_OBJECT_METHOD` macro in `OBJECT_CONSTRUCTORS`, and today enforced across all `V8_OBJECT` classes by `tools/gcmole`). If a GC were triggered inside an object method, `this` could be evacuated to a new page, leaving the C++ `this` pointer dangling into swept/reused memory.
* **Pattern**: Any operation on `Foo` that might allocate on the heap **must** be declared as a `static` method accepting `Isolate*` and `DirectHandle<Foo>` (or `Handle<Foo>`) instead of using `this`:
  ```cpp
  // BAD: `this` is a raw HeapObject* that becomes invalid if AllocateFixedArray triggers GC!
  void MyObject::GrowBackingStore(Isolate* isolate, int new_cap) { ... }

  // GOOD: Static method taking DirectHandle<MyObject> so `self` is updated/pinned across GC.
  static void MyObject::GrowBackingStore(Isolate* isolate,
                                         DirectHandle<MyObject> self,
                                         int new_cap);
  ```
* **`DisableGCMole` (`src/common/assert-scope.h`)**: Suppresses `gcmole`'s dead-variable analysis in rare false-positive cases where C++ control flow guarantees two branches cannot both allocate and use a raw `Tagged<T>`.
