# The Zone Allocator (`src/zone/`)

In addition to the garbage-collected JavaScript `Heap`, V8 uses an arena-style bump-pointer allocator called **`Zone`** (`src/zone/zone.h`) for temporary C++ data structures whose lifetimes are tied to a well-defined compilation or parsing phase (such as AST nodes in the parser, bytecode generation structures in Ignition, and IR graphs/register-allocation tables in Maglev and TurboFan/Turboshaft).

---

## 1. Core Design: Fast Bump Allocation + Bulk Reclamation

A `Zone` allocates C++ memory from chained chunks called **`Segment`s** (`src/zone/zone-segment.h`) obtained from an **`AccountingAllocator`** (`src/zone/accounting-allocator.h`):

```
Zone
+-------------------+      Segment (Head)                        Segment (Previous)
| position_ --------+---+  +------------------------------+      +--------------------+
| limit_ -----------+---|->| [Allocated...] [pos .. limit)|----->| [Full Segment]     |
| segment_head_ ----+-->+  +------------------------------+      +--------------------+
| allocator_        |
+-------------------+
```

* **Fast-Path Bump Allocation (`Zone::Allocate<T>(size)`)**:
  * Rounds `size` up to `kAlignmentInBytes` (`8` bytes on all platforms).
  * Checks `if (size <= limit_ - position_)`, advances `position_ += size`, and returns the old `position_`.
  * Has zero per-object header overhead and requires no atomic operations (a `Zone` is strictly single-threaded, owned by one main or background compilation thread at a time).
* **Slow-Path Segment Expansion (`Zone::Expand`)**:
  * When the current `Segment` is full, `Zone::Expand` requests a new `Segment` from `AccountingAllocator`.
  * Standard segment sizes grow geometrically in powers of two starting at `Zone::kMinimumSegmentSize = 8 * KB` up to `Zone::kMaximumSegmentSize = 32 * KB` (capped at `std::max(min_new_size, kMaximumSegmentSize)`, with `ManagedZones::AllocateSegment` enforcing a hard `kMaxSize = 2 * GB` cap).
* **Bulk Destruction (`~Zone()`)**:
  * Destroying a `Zone` releases all of its `Segment`s back to `AccountingAllocator` at once without invoking destructors on individual `ZoneObject`s (unless a container explicitly manages destruction).

---

## 2. `AccountingAllocator` and `ManagedZones` Segment Pooling (`src/zone/accounting-allocator.h`)

All `Zone`s in an `Isolate` (plus background compilation jobs) obtain their `Segment`s through `AccountingAllocator`:
1. **Memory Accounting**: Atomically tracks `current_memory_usage_` and `max_memory_usage_` across all active zones (`GetCurrentMemoryUsage()`, `GetMaxMemoryUsage()`).
2. **`ManagedZones` Virtual Memory Pooling (`v8_flags.managed_zone_memory`)**:
   * Compiler passes create and destroy thousands of short-lived zones per second. Repeatedly calling `malloc`/`free` or `mmap`/`munmap` for large segments causes OS page-table contention.
   * When `v8_flags.managed_zone_memory` is enabled, `ManagedZones::AllocateSegment` rounds requests up to a multiple of `kMinZonePageSize` (**512KB** on 64-bit, **256KB** on 32-bit), pools exact `kMinZonePageSize` `VirtualMemory` reservations via **`IsolateGroup::current()->memory_pool()`** (`RemoveZoneReservation` / `AddZoneReservation`), and placement-constructs `new (memory.ptr) Segment(memory.count)` directly at the start of the reservation.
3. **`TracingAccountingAllocator` & `TypeStats` (`--trace-zone-stats`, `--trace-zone-type-stats` / `V8_ENABLE_PRECISE_ZONE_STATS`)**:
   * When precise zone stats are enabled (`V8_ENABLE_PRECISE_ZONE_STATS`), `Zone::New<T>` and `Zone::Allocate<TypeTag>(size)` tag every allocation with its C++ `TypeTag`, allowing `TypeStats` (`src/zone/type-stats.h`) to report exact per-type peak and current byte usage across compiler phases.

---

## 3. Lifecycle Controls: `Delete`, `Reset`, and `Seal` (`src/zone/zone.h`)

### 1. `Zone::Delete<T>(void* pointer, size_t size)` and `Zone::DeleteArray<T>(T* pointer, size_t length)`
While most `Zone` allocations live until the entire `Zone` is destroyed, containers such as `ZoneVector` or `ZoneAbslFlatHashMap` call `Zone::DeleteArray` / `Zone::Delete` when resizing backing buffers:
* **Does Not Fragment the Bump Pointer**: `Zone::Delete` does not maintain a general-purpose free list inside the zone.
* **Debug Zapping**: In `DEBUG` builds, `Zone::Delete` overwrites the deleted memory region `[pointer, pointer + size)` with **`kZapDeadByte` (`0xcd`)** so dangling pointer reads fail immediately and loudly.
* **Precise Stats Accounting**: Under `V8_ENABLE_PRECISE_ZONE_STATS`, `Zone::Delete<TypeTag>` decrements the per-type active allocation counters (`allocation_size_for_tracing_`, `freed_size_for_tracing_`).
* **AddressSanitizer Integration (`V8_USE_ADDRESS_SANITIZER`)**:
  * When compiled with ASan (`V8_USE_ADDRESS_SANITIZER`), `Zone::Allocate` redirects to `Zone::AsanNew(size)`, which appends a **24-byte redzone (`kASanRedzoneBytes`)** to every individual zone allocation and poisons the redzone via `ASAN_POISON_MEMORY_REGION`.
  * Out-of-bounds reads or writes between adjacent objects inside the same `Segment` are therefore caught immediately by ASan, and segments are unpoisoned (`ASAN_UNPOISON_MEMORY_REGION`) when released or reset.

### 2. `Zone::Reset()`
Iterative algorithms (such as loop-by-loop or phase-by-phase compiler analyses) often need a scratch zone that is cleared after every iteration:
* Calling **`zone.Reset()`** releases all allocated `Segment`s back to `AccountingAllocator` **except the current `segment_head_`**:
  * It keeps `segment_head_` attached to the `Zone`, unpoisons its memory under ASan, zaps it in `DEBUG` builds (`Segment::ZapContents`), and resets `position_ = segment_head_->start()` and `limit_ = segment_head_->end()`.
* Subsequent iterations reuse the already-allocated `segment_head_` with zero `AccountingAllocator` or OS overhead.

### 3. `Zone::Seal()`
Once a compiler phase finishes building an immutable data structure in a `Zone` (for example, an AST or bytecode metadata that will only be read by subsequent phases):
* Calling **`zone.Seal()`** sets `sealed_ = true`.
* Any subsequent attempt to call `Zone::Allocate` in a sealed `Zone` fails immediately with `CHECK(!sealed_)`, preventing accidental memory growth or phase-ordering bugs.

---

## 4. Using `Zone` in C++ (`ZoneObject` & Containers)

### 1. `ZoneObject` (`src/zone/zone.h`)
Classes intended to be allocated inside a `Zone` inherit from `ZoneObject`:
```cpp
class AstNode : public ZoneObject {
 public:
  explicit AstNode(int pos) : position_(pos) {}
 private:
  int position_;
};

// Allocation via Zone::New<T>(args...):
AstNode* node = zone->New<AstNode>(42);
```
`ZoneObject` deletes `operator delete` so callers cannot accidentally pass a `ZoneObject*` to global `delete`. Note that **`~T()` is never called automatically when the `Zone` is destroyed**, so `ZoneObject` subclasses must not own non-Zone heap resources (`std::vector`, `std::unique_ptr`, file descriptors) unless explicitly cleaned up before zone destruction.

### 2. Zone-Backed Containers (`src/zone/zone-containers.h`)
For variable-sized collections inside a `Zone`, V8 provides STL and Abseil container wrappers parameterized with `ZoneAllocator<T>`:
* **`ZoneVector<T>`**: Custom vector optimized for `Zone` memory (calls `Zone::DeleteArray` on its old buffer when growing, and supports trivial-copy fast paths).
* **`ZoneLinkedList<T>`**, **`ZoneDeque<T>`**, **`ZoneQueue<T>`**, **`ZoneStack<T>`**.
* **`ZoneMap<K, V>`**, **`ZoneSet<K>`**, **`ZoneMultimap<K, V>`** (`std::map` / `std::set` backed by `ZoneAllocator`).
* **`ZoneAbslFlatHashMap<K, V>`**, **`ZoneAbslFlatHashSet<K>`**, **`ZoneAbslBTreeMap<K, V>`**: High-performance Abseil flat hash tables and B-trees backed by `ZoneAllocator`.
* **`zone->AllocateVector<T>(length)`**, **`zone->NewVector<T>(length, [value])`**, and **`zone->CloneVector<T>(v)`**: Return a contiguous `base::Vector<T>` slice allocated directly in the `Zone` (uninitialized for `AllocateVector`, default- or value-constructed for `NewVector`, and copied for `CloneVector`).

> [!WARNING]
> **Never Store Raw `Tagged<T>` or `DirectHandle<T>` Inside a `ZoneObject` Across GC**:
> The V8 garbage collector **never** scans `Zone` memory. If a `ZoneObject` lives across a safepoint where GC can run (such as on the main thread during compilation finalize), any reference to a V8 `HeapObject` stored inside the `ZoneObject` **must** be an **`IndirectHandle<T>` (`Handle<T>`)** backed by a `HandleScope` or `PersistentHandlesScope`.
