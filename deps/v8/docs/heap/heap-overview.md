# V8 Heap Overview

The V8 `Heap` (`src/heap/heap.h`) manages all garbage-collected memory for a JavaScript `Isolate`. Its design combines a **generational hypothesis** (most objects die young), **segregated spaces** tailored to object mutability, size, thread sharing, and security trust levels, and a **sandbox-hardened page metadata architecture** that separates untrusted in-sandbox headers from out-of-sandbox GC control structures.

---

## 1. Page Architecture and the Sandbox Metadata Split

V8 divides heap memory into aligned **Pages** (`NormalPage`) for regular objects and variable-sized **Large Pages** (`LargePage`) for oversized allocations:
* **Uniform Page Size**: Every regular page is uniformly **256KB** across all architectures (`kRegularPageSize = 1 << kPageSizeBits` where `kPageSizeBits = 18` in `src/common/globals.h`; `NormalPage::kPageSize = 256KB`).
* **Large Object Threshold**: Objects larger than `kMaxRegularHeapObjectSize` (`128KB`, half a regular page) are allocated on dedicated `LargePage`s rather than in regular paged spaces.

### In-Sandbox `MemoryChunk` vs. Out-of-Sandbox `BasePage` / `MutablePage`
Because every regular page is aligned to `256KB` (`kAlignment = 256KB`), V8 can locate the page header for any `HeapObject` in a single bitwise `AND` operation (`address & ~kAlignmentMask`, implemented by `MemoryChunk::FromHeapObject(obj)`).

However, for spaces inside the [V8 Sandbox](../sandbox/architecture.md), the start of the 256KB page lies inside attacker-writable sandbox memory. If GC metadata (such as `Heap*`, `Space*`, free lists, or remembered sets) were stored directly at the start of the page, an attacker could corrupt them to achieve arbitrary out-of-sandbox code execution. V8 prevents this by splitting page headers into two halves:

```
Inside Sandbox (256KB-aligned Page Base)          Outside Sandbox (Trusted C++ Heap)
+--------------------------------------+          +-----------------------------------------+
| MemoryChunk (src/heap/memory-chunk.h)|          |IsolateGroup::metadata_pointer_table()   |
| - untrusted_main_thread_flags_       |          |  [metadata_index_] -------------------+ |
| - metadata_index_ (uint32_t) --------+--------->+---------------------------------------|-+
+--------------------------------------+                                                  |
| Object Area (area_start .. area_end) |                                                  v
|   HeapObject, HeapObject, ...        |          +-----------------------------------------+
+--------------------------------------+          | BasePage (src/heap/base-page.h)         |
                                                  | - chunk_ (back-pointer to MemoryChunk*) |
                                                  | - heap_, owner_ (Space*), size_         |
                                                  | - area_start_, area_end_, flags_        |
                                                  +-----------------------------------------+
                                                  | MutablePage (src/heap/mutable-page.h)   |
                                                  | - trusted_main_thread_flags_            |
                                                  | - marking_bitmap_                       |
                                                  | - slot_set_[8], typed_slot_set_[8]      |
                                                  | - categories_ (FreeListCategory*)       |
                                                  | - live_byte_count_, mutex_              |
                                                  +-----------------------------------------+
                                                    ^                                     ^
                                                    |                                     |
                                              NormalPage (256KB)              LargePage (Variable)
```

1. **`MemoryChunk` (`src/heap/memory-chunk.h`, In-Sandbox)**:
   * Located at offset `0` of the aligned page (`MemoryChunk::FromAddress(a)`).
   * Contains **only two fields**:
     1. `untrusted_main_thread_flags_` (`MainThreadFlags`): Bitflags queried on hot inline paths (such as `POINTERS_FROM_HERE_ARE_INTERESTING`, `POINTERS_TO_HERE_ARE_INTERESTING`, `IN_WRITABLE_SHARED_SPACE`, `FROM_PAGE`, `TO_PAGE`, `EVACUATION_CANDIDATE`). Code reading these flags must treat them as **untrusted hints**; corrupting them may never violate memory safety outside the sandbox (for example, page executability `IsExecutableField` is stored only in the trusted `BasePage::flags_`).
     2. `metadata_index_` (`uint32_t`, under `V8_ENABLE_SANDBOX`) or `metadata_` (`BasePage*`, when the sandbox is disabled).
   * **Validated Metadata Lookup (`MemoryChunk::Metadata()` in `src/heap/memory-chunk-inl.h`)**:
     * Indexes into `IsolateGroup::current()->metadata_pointer_table()[metadata_index_ & kMetadataPointerTableSizeMask]`.
     * Verifies that the `BasePage` belongs to the current `Isolate`, `SharedSpace`, or `ReadOnlySpace` (`CheckIfMetadataAccessibleFromIsolate`).
     * Executes `CHECK_EQ(metadata->Chunk(), this)` to ensure an attacker did not overwrite `metadata_index_` to point to another page's `BasePage`.

2. **Out-of-Sandbox Page Metadata Hierarchy**:
   * **`BasePage` (`src/heap/base-page.h`)**: Base metadata class for all pages. Stores trusted `heap_`, `owner_` (`Space*`), `chunk_` (`MemoryChunk*`), `size_`, `area_start_`, `area_end_`, `allocated_bytes_`, `wasted_memory_`, virtual memory `reservation_`, and trusted `flags_` (`IsReadOnlyPageField`, `IsLargePageField`, `IsExecutableField`, `IsTrustedField`, `IsWritableSharedSpaceField`, `IsEvacuationCandidateField`, `IsQuarantinedField`, `WillBePromotedField`, `IsBlackAllocated`).
   * **`ReadOnlyPage` (`src/heap/read-only-spaces.h`)**: Final subclass of `BasePage` for immutable pages in `ReadOnlySpace` (no marking bitmap, free list, or remembered sets).
   * **`MutablePage` (`src/heap/mutable-page.h`)**: Subclass of `BasePage` for all garbage-collected pages. Owns:
     * `trusted_main_thread_flags_` (authoritative copy of `MainThreadFlags`, synced to `MemoryChunk::untrusted_main_thread_flags_`),
     * `marking_bitmap_` (`MarkingBitmap`, 1 bit per `kTaggedSize` word on the page),
     * `slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES]` and `typed_slot_set_[NUMBER_OF_REMEMBERED_SET_TYPES]`,
     * `categories_` (`FreeListCategory**` for `OldSpace` free-list allocation),
     * `live_byte_count_`, `active_system_pages_`, `marking_progress_tracker_` (for chunked incremental marking of large arrays), and `mutex_` / `object_mutex_`.
   * **`NormalPage` (`src/heap/normal-page.h`)**: Final subclass of `MutablePage` representing a standard 256KB page.
   * **`LargePage` (`src/heap/large-page.h`)**: Final subclass of `MutablePage` containing a single large `HeapObject` (`GetObject()`).

---

## 2. Heap Spaces

V8 partitions the heap into specialized spaces (`AllocationSpace` enum in `src/common/globals.h`), implemented in `src/heap/new-spaces.h`, `src/heap/paged-spaces.h`, `src/heap/large-spaces.h`, and `src/heap/read-only-spaces.h`.

| Space Class | `AllocationSpace` | Location | Description |
| :--- | :--- | :--- | :--- |
| **`SemiSpaceNewSpace`** | `NEW_SPACE` | In-Sandbox (`MainCage`) | Default copying nursery for the [Scavenger](garbage-collection.md), split into two `SemiSpace`s (`to_space_` and `from_space_`). |
| **`PagedNewSpace`** | `NEW_SPACE` | In-Sandbox (`MainCage`) | Non-copying paged nursery (`PagedSpaceForNewSpace`) used when Minor Mark-Sweep (`--minor-ms`) is enabled. |
| **`StickySpace`** | `OLD_SPACE` | In-Sandbox (`MainCage`) | Unified young+old paged space (`StickySpace : public OldSpace`) used when `v8_flags.sticky_mark_bits` is enabled. Retains markbits after GC to distinguish old objects (marked) from newly allocated young objects (unmarked). |
| **`OldSpace`** | `OLD_SPACE` | In-Sandbox (`MainCage`) | Tenured objects that survived young generation GC or were pretenured (`AllocationType::kOld`). Managed by Mark-Sweep-Compact (`MarkCompactCollector`). |
| **`CodeSpace`** | `CODE_SPACE` | Outside Sandbox (`CodeRange`) | Executable `InstructionStream` objects (`W^X` protected via `RwxMemoryWriteScope`). Note that `Code` metadata objects live in `TrustedSpace`. |
| **`TrustedSpace`** | `TRUSTED_SPACE` | Outside Sandbox (`TrustedCage`) | Security-sensitive `TrustedObject`s (`BytecodeArray`, `Code`, `ProtectedFixedArray`, `TrustedFixedArray`, `WasmTrustedInstanceData`, etc.). |
| **`SharedSpace`** | `SHARED_SPACE` | In-Sandbox (`MainCage`) | Objects shared across multiple worker `Isolate`s in the same `IsolateGroup` (e.g., shared strings, `JSSharedStruct`, `JSSharedArray`). |
| **`SharedTrustedSpace`** | `SHARED_TRUSTED_SPACE` | Outside Sandbox (`TrustedCage`) | `TrustedObject`s shared across multiple `Isolate`s in the same `IsolateGroup` (e.g., shared Wasm dispatch tables and instance data). |
| **`ReadOnlySpace`** | `RO_SPACE` | Bottom of `MainCage` | Immortal, immutable objects (`MetaMap`, oddballs like `undefined`/`null`, canonical empty arrays) deserialized from the startup snapshot and shared across all `Isolate`s via `ReadOnlyHeap`. |
| **`NewLargeObjectSpace`** | `NEW_LO_SPACE` | In-Sandbox (`MainCage`) | Young generation allocations larger than `kMaxRegularHeapObjectSize`. Promoted in-place to `OldLargeObjectSpace` by relinking the `LargePage` without copying bytes. |
| **`OldLargeObjectSpace`** | `LO_SPACE` | In-Sandbox (`MainCage`) | Tenured large objects (`> kMaxRegularHeapObjectSize`). Never moved by compaction. |
| **`CodeLargeObjectSpace`** | `CODE_LO_SPACE` | Outside Sandbox (`CodeRange`) | Large executable `InstructionStream` objects (up to `LargePage::kMaxCodePageSize = 512MB`). |
| **`TrustedLargeObjectSpace`** | `TRUSTED_LO_SPACE` | Outside Sandbox (`TrustedCage`) | Large `TrustedObject`s (`TrustedByteArray`, large `BytecodeArray`s, large `TrustedFixedArray`s). |
| **`SharedLargeObjectSpace`** | `SHARED_LO_SPACE` | In-Sandbox (`MainCage`) | Large shared objects across `Isolate`s. |
| **`SharedTrustedLargeObjectSpace`** | `SHARED_TRUSTED_LO_SPACE` | Outside Sandbox (`TrustedCage`) | Large shared `TrustedObject`s across `Isolate`s. |

---

## 3. Allocation: `MainAllocator` and `LinearAllocationArea` (LABs)

To make allocation in both JIT code and C++ as fast as a stack push, V8 uses **Linear Allocation Buffers (LABs)** managed by **`MainAllocator`** (`src/heap/main-allocator.h`) and **`LinearAllocationArea`** (`src/heap/linear-allocation-area.h`).

### `LinearAllocationArea` (`[start_, top_, limit_]`)
Each `MainAllocator` (one per active space on the main thread, and per `LocalHeap` / `CompactionSpace` on background threads) holds a `LinearAllocationArea` with three pointers (`start <= top <= limit`):
* **`top_`**: Current bump pointer. Fast-path allocation checks `if (top_ + size <= limit_)`, returns the old `top_`, and advances `top_ += size`.
* **`limit_`**: End of the current contiguous allocation window.
* **`start_`**: Tracks where continuous bump allocation started since the last LAB reset or `AllocationObserver` step.

```
NormalPage Object Area
+-----------------------+-----------------------------------+-------------------+
| Previously Allocated  | Active LinearAllocationArea (LAB) | Free / Swept      |
| Objects               | [start_ ... top_ ....... limit_)  | Categories        |
+-----------------------+-----------------------------------+-------------------+
                                      |---> bump allocation
```

### `MainAllocator` and `AllocatorPolicy`
When `top_ + size > limit_`, `MainAllocator::AllocateRawSlow` invokes its space-specific **`AllocatorPolicy`**:
1. **`SemiSpaceNewSpaceAllocatorPolicy`**: Retires the current LAB and advances to the next 256KB `NormalPage` in `to_space_` (or triggers a minor GC if `to_space_` is full).
2. **`PagedSpaceAllocatorPolicy`** (used by `OldSpace`, `CodeSpace`, `TrustedSpace`, `SharedSpace`): Searches the space's size-segregated **`FreeList`** (`FreeListManyCached` with 24 size categories from 24 bytes up to 128KB) to find a free block, sets `[start_, top_, limit_]` to that block, and returns the excess to the free list when the LAB is retired. If the free list cannot satisfy the request, it sweeps an unswept page via `Sweeper`, expands the space with a new `NormalPage`, or triggers a GC.
3. **`PagedNewSpaceAllocatorPolicy`** (used by MinorMS `PagedNewSpace`): Combines paged free-list allocation with young-generation capacity limits.

### Key LAB Operations
* **Making the Heap Iterable (`MakeLinearAllocationAreaIterable()`)**: The unused region `[top_, limit_)` inside an active LAB contains uninitialized memory. Before any heap walk, root scan, or GC, `MainAllocator::MakeLinearAllocationAreaIterable()` writes a valid V8 **`Filler`** object (`OnePointerFillerMap`, `TwoPointerFillerMap`, or `FreeSpace` with its size) into `[top_, limit_)` so object iterators can step across the gap safely.
* **Black Allocation (`MarkLinearAllocationAreaBlack()`)**: When incremental marking is active in tenured spaces (`v8_flags.black_allocated_pages`), newly allocated objects must be born black so they survive the current GC cycle.
* **Allocation Observers (`AllocationCounter`)**: Profilers, incremental marking steps, and stress modes register `AllocationObserver`s on `MainAllocator`. Rather than checking observers on every allocation, `MainAllocator` lowers `limit_` to `min(physical_end, top_ + next_observer_step_bytes)`. When `top_` hits this lowered `limit_`, the slow path invokes the observers, advances `start_ = top_`, and restores or recomputes `limit_`.
* **Concurrent Readers (`LinearAreaOriginalData`)**: Background compiler threads can call `MainAllocator::IsPendingAllocation(object_address)` (guarded by `linear_area_original_data_`) to check whether an object is still inside an un-flushed LAB whose original bounds were `[original_top_, original_limit_)`.

---

## 4. Key Source Files

* `src/heap/heap.h`, `src/heap/heap.cc` - Central `Heap` coordinator.
* `src/heap/heap-allocator.h`, `src/heap/main-allocator.h`, `src/heap/linear-allocation-area.h` - Allocation entry points (`HeapAllocator::AllocateRawWith<...>`) and LAB management.
* `src/heap/memory-chunk.h`, `src/heap/base-page.h`, `src/heap/mutable-page.h`, `src/heap/normal-page.h`, `src/heap/large-page.h` - In-sandbox `MemoryChunk` and out-of-sandbox page metadata classes.
* `src/heap/new-spaces.h`, `src/heap/paged-spaces.h`, `src/heap/large-spaces.h`, `src/heap/read-only-spaces.h` - Heap space implementations.
* `src/heap/heap-layout.h`, `src/heap/heap-layout-inl.h` - Fast predicates for querying object space membership (`InYoungGeneration`, `InReadOnlySpace`, `InWritableSharedSpace`, `IsOwnedByAnyHeap`).
