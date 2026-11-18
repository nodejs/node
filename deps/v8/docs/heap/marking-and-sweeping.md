# Marking and Sweeping in V8

Major garbage collection in V8 (`MarkCompactCollector` in `src/heap/mark-compact.h`) and Minor Mark-Sweep (`MinorMarkSweepCollector` in `src/heap/minor-mark-sweep.h`) rely on **tri-color incremental and concurrent marking** followed by **concurrent page sweeping**.

---

## 1. Tri-Color Marking Abstraction

V8 tracks object liveness using a per-page **`MarkingBitmap`** (`src/heap/marking.h`) stored in the out-of-sandbox **`MutablePage`** metadata (`1 bit` per `kTaggedSize` word on the 256KB page) combined with **Marking Worklists** (`src/heap/marking-worklist.h`):

* **White (Unvisited)**: The object's markbit in `MarkingBitmap` is `0` and the object is not in any marking worklist. At the start of marking, all objects (except those on black-allocated pages) are white. Any object that remains white after the transitive closure completes is unreachable garbage and will be reclaimed by the sweeper.
* **Grey (Discovered, Pending Scan)**: The object's markbit in `MarkingBitmap` has been atomically transitioned from `0` to `1` (`TryMark`), and the object has been pushed onto a **`MarkingWorklist`** waiting for a marker thread to visit its fields.
* **Black (Visited and Scanned)**: The object's markbit is `1` and it has been popped from the `MarkingWorklist` with all of its outgoing pointers visited. A black object cannot directly point to a white object unless a write barrier intervenes.

```
[White: markbit = 0]
        |
        | Atomic TryMark (0 -> 1) + Push to MarkingWorklist
        v
[Grey:  markbit = 1, in MarkingWorklist]
        |
        | Pop from MarkingWorklist + Visit all outgoing fields
        v
[Black: markbit = 1, not in MarkingWorklist]
```

> [!NOTE]
> Because V8 uses a **single markbit per object** in `MarkingBitmap` rather than two bits, the distinction between **Grey** and **Black** is simply whether the marked object is still queued inside a `MarkingWorklist` (or has unvisited chunks tracked by `MarkingProgressTracker` for large arrays).

---

## 2. Incremental and Concurrent Marking

To avoid long stop-the-world pauses on large heaps, marking is interleaved with JavaScript execution (`IncrementalMarking` in `src/heap/incremental-marking.h`) and offloaded to background worker threads (`ConcurrentMarking` in `src/heap/concurrent-marking.h`).

### 1. Incremental Marking (Main Thread)
* Triggered when old-generation memory usage crosses a dynamic threshold computed by `HeapLimit` / `MemoryReducer`.
* The main thread performs small marking increments (`IncrementalMarking::AdvanceWithDeadline`) driven by **allocation observers** on `MainAllocator` (proportional to bytes allocated) and task-runner steps.
* **Black Allocation (`v8_flags.black_allocated_pages`)**: Once incremental marking starts, pages allocated in tenured spaces (`OldSpace`, `CodeSpace`, `TrustedSpace`) are marked with `IsBlackAllocated` so newly allocated objects are born black and do not need individual markbit updates.

### 2. Concurrent Marking (Background Worker Threads)
* `ConcurrentMarking` spawns V8 job tasks that pop grey objects from thread-local segments of `MarkingWorklists` and scan their `BodyDescriptor` fields concurrently while the main thread executes JavaScript.
* **Data-Race Prevention**:
  * Reading object maps and fields from background marker threads uses relaxed or acquire atomic loads (`Object::Relaxed_Load` / `Acquire_Load`).
  * **In-Place Map Transitions / Layout Changes**: If the main thread mutates an object's layout in-place (for example, right-trimming or left-trimming a `FixedArray` or transitioning an unboxed field), it either synchronizes with `object_mutex_` on the `MutablePage` or pushes the object to the main-thread **On-Hold Worklist** (`on_hold`) when concurrent marking encounters an object inside a pending `LinearAllocationArea` (`MainAllocator::IsPendingAllocation`).
* **Chunked Marking for Large Arrays (`MarkingProgressTracker`)**: Scanning a multi-megabyte `FixedArray` in a single step would stall a marker thread. `MarkingProgressTracker` (stored in `MutablePage`) splits large arrays into `kChunkSize` (`32KB`) slices so marker threads scan one chunk at a time and re-push the array until all chunks are visited.

### 3. Write Barriers Maintaining the Tri-Color Invariant
While JavaScript runs concurrently with marking, mutating `host->field = value` could store a pointer to a **White** `value` inside an already-scanned **Black** `host`, hiding `value` from the marker.

V8 enforces the **strong tri-color invariant** using a **Dijkstra-style Insertion Write Barrier**:
* When marking is active, `POINTERS_FROM_HERE_ARE_INTERESTING` and `POINTERS_TO_HERE_ARE_INTERESTING` are set on all `MemoryChunk` headers.
* On any pointer store (`WriteBarrier::ForValue`, `ForTrustedPointer`, `ForProtectedPointer`, `ForJSDispatchHandle`, `ForExternalPointer`, `ForCppHeapPointer`), if `value` is white, the write barrier atomically marks `value` (`0 -> 1`) and pushes it onto the `MarkingWorklist` (turning it **Grey**).
* For **indirect table entries** (`TrustedPointerTable`, `JSDispatchTable`, `ExternalPointerTable`), the write barrier also sets the markbit on the table entry itself so table sweeping keeps the entry alive.

---

## 3. Marking Worklists (`src/heap/marking-worklist.h`)

To minimize synchronization overhead between the main thread and concurrent marker threads, `MarkingWorklists` maintains worklists built out of lock-free segmented buffers (`base::Worklist`, 64 entries per segment). Each thread (the main thread and each concurrent `ConcurrentMarking` worker) operates on a private `MarkingWorklists::Local` view and publishes full segments to the global pool for work-stealing.

1. **`shared_` (Default Worklist)**: The primary worklist holding grey `HeapObject`s that can be scanned by either the main thread or any concurrent worker thread.
2. **`on_hold_` (On-Hold Worklist)**: When a concurrent marker thread encounters a grey object whose address falls inside the main thread's current un-flushed `LinearAllocationArea` (`MainAllocator::IsPendingAllocation`), the concurrent thread **cannot** safely read the object's `Map` or fields yet because the main thread may still be initializing them without release fences. The worker pushes the object onto `on_hold_`, and the main thread merges `on_hold_` back into `shared_` the next time it flushes its LABs.
3. **`other_` / Context-Specific Worklists (`context_worklists_`)**: Used when native-context memory measurement (`MeasureMemory`) is active to attribute retained memory sizes to specific `NativeContext`s.
4. **Ephemeron Tracking (`discovered_ephemerons_`, `current_ephemerons_`, `next_ephemerons_`)**: Special worklists for **`EphemeronHashTable`** (backing `WeakMap` and `WeakSet`). An ephemeron entry `(key, value)` only marks `value` if `key` is already marked; otherwise the pair is queued in `next_ephemerons_` and re-evaluated during weak-closure fixpoint iteration.

---

## 4. Final Atomic Pause & Weak Closure (`MarkCompactCollector`)

Once concurrent and incremental marking have drained almost all grey objects, the collector enters a brief stop-the-world safepoint (`IsolateSafepointScope`):
1. **Root Re-Scanning**: Scans the C++ stack (via Conservative Stack Scanning if active) and `HandleScope`s, which were not tracked by write barriers during execution.
2. **Transitive Closure & Unified Heap Fixpoint**: Drains remaining `shared_`, `on_hold_`, ephemeron worklists, and `CppHeap` (Oilpan) worklists until a global fixpoint is reached.
3. **Weak Reference Clearing (`ClearNonLiveReferences`)**:
   * Weak object slots (`Tagged<Weak<T>>`) pointing to white objects are overwritten with `ClearedWeakValue` (`0x3`) or migrated if the target is evacuated.
   * Weak entries in `TransitionArray`, `DescriptorArray`, `SharedFunctionInfo`, `Script` lists, and string tables are pruned.
   * Unmarked entries in `TrustedPointerTable`, `JSDispatchTable`, and `ExternalPointerTable` are freed.

---

## 5. Sweeping (`src/heap/sweeper.h`)

After marking completes, **Sweeping** reclaims unreachable (white) objects on non-evacuated `NormalPage`s and returns their memory to the space's size-segregated **`FreeList`** (`src/heap/free-list.h`):

1. **Lazy & Concurrent Sweeping**:
   * The main thread does **not** sweep pages during the atomic pause. Instead, `Sweeper::StartSweeping` schedules background `SweeperJob` worker threads (`ConcurrentSweeper`).
   * If the main thread needs to allocate in a space before background sweepers have produced enough free-list memory, `Sweeper::ParallelSweepSpace` sweeps one or more pages on demand in parallel with the background workers.
2. **How `RawSweep` Works on a `NormalPage`**:
   * Walks the page's `MarkingBitmap` word by word.
   * **Live (Black) Objects**: Advances past `object->Size()`, sums `live_bytes`, and (unless `--sticky-mark-bits` is enabled) clears the markbits back to `0` for the next GC cycle.
   * **Dead (White) Ranges**: Contiguous unmarked memory ranges `[free_start, free_end)` are overwritten with a valid **`FreeSpace` / `Filler`** map header (and zapped with `kZapValue` if `--zap-garbage` is enabled) and linked into the page's `FreeListCategory` buckets according to size.
   * **Remembered-Set & Slot Cleanup**: Any recorded `OLD_TO_NEW` or `OLD_TO_SHARED` slots falling inside dead `[free_start, free_end)` ranges are cleared from the page's `SlotSet` and `TypedSlotSet` (and empty `SlotSet` buckets are freed).
   * **Completely Empty Pages**: If a page has `0` live bytes after sweeping, it is unlinked from the space and returned to the `MemoryAllocator` / OS page pool.
